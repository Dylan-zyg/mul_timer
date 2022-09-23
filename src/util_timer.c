#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "util_rbtree.h"
#include "util_queue.h"
#include "util_timer.h"

#define TMPBUF_LEN 20
#define THREAD_MAX 20

typedef int (*Handler)(void *, void *, int);

typedef struct
{
	int fd[2];
	int run;
	pthread_t id;
	util_rbtree_t vnode_tree;
	base_queue_t cache_queue;
	pthread_mutex_t queue_mutex;
	pthread_mutex_t tree_mutex;
} Timer;

typedef struct
{
	base_queue_t node;
	void *para;
	int cmd;
	uint64_t id;
	uint64_t val;
	uint64_t interval;
	char repeat;
	void *tmpbuf;
	size_t buf_len;
	Handler handler;
} VirtualNode;

typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	uint16_t thread_num;
	pthread_t tid[THREAD_MAX];
	char run;
	base_queue_t task_queue;
	Timer *timer;
} ThreadPool;

static void UtilClose(Timer *timer);
static void UtilRemoveVir(Timer *timer, VirtualNode *vir);
static ThreadPool *g_thread_pool = NULL;
static Timer *g_timer = NULL;

static Timer *getTimerhandel()
{
	return g_timer;
}

static void ThreadPoolClean(ThreadPool *thpool)
{
	base_queue_t *data;
	VirtualNode *vir;

	
	pthread_mutex_lock(&thpool->mutex);
	while (!base_queue_empty(&thpool->task_queue))
	{
		data = base_queue_head(&thpool->task_queue);
		base_queue_remove(data);
		vir = (VirtualNode *)data;
		if (0 == vir->repeat)
		{
			UtilRemoveVir(thpool->timer, vir);
		}
	}
	pthread_mutex_unlock(&thpool->mutex);
}

static void ThreadPoolDestroy(ThreadPool *thpool)
{
	int i;

	if (NULL == thpool)
	{
		return;
	}

	thpool->run = 0;
	ThreadPoolClean(thpool);

	pthread_cond_broadcast(&thpool->cond);
	for (i = 0; i < thpool->thread_num; i++)
	{
		pthread_join(thpool->tid[i], NULL);
	}

	pthread_mutex_destroy(&thpool->mutex);
	pthread_cond_destroy(&thpool->cond);

	free(thpool);
}

static int AddTask(ThreadPool *thpool, base_queue_t *data)
{
	if (NULL == thpool || NULL == data)
	{
		return -1;
	}

	
	pthread_mutex_lock(&thpool->mutex);
	base_queue_insert_tail(&thpool->task_queue, data);
	pthread_cond_signal(&thpool->cond);
	pthread_mutex_unlock(&thpool->mutex);

	return 0;
}

static void *ThreadFunc(void *argv)
{
	ThreadPool *thpool = (ThreadPool *)argv;
	base_queue_t *node;
	VirtualNode *vir;

	while (thpool->run)
	{
		
		pthread_mutex_lock(&thpool->mutex);
		if (base_queue_empty(&thpool->task_queue))
		{
			pthread_cond_wait(&thpool->cond, &thpool->mutex);
		}
		if (!base_queue_empty(&thpool->task_queue))
		{
			node = base_queue_head(&thpool->task_queue);
			base_queue_remove(node);
			pthread_mutex_unlock(&thpool->mutex);

			vir = (VirtualNode *)node;
			if (vir->handler)
			{
				vir->handler(vir->para, vir->tmpbuf, vir->cmd);
			}
			if (0 == vir->repeat)
			{
				/* recycle to pool queue */
				UtilRemoveVir(thpool->timer, vir);
			}
		}
		else
		{
			pthread_mutex_unlock(&thpool->mutex);
		}
	}
	pthread_exit(NULL);
}

static ThreadPool *ThreadPoolInit(Timer *timer, uint16_t num)
{
	ThreadPool *thpool;
	int i;

	if (num <= 1)
	{
		return NULL;
	}
	if (num > THREAD_MAX)
	{
		num = THREAD_MAX;
	}

	thpool = (ThreadPool *)malloc(sizeof(ThreadPool));
	if (!thpool)
	{
		printf("Thread pool malloc failed\n");
		return NULL;
	}
	base_queue_init(&thpool->task_queue);
	pthread_mutex_init(&thpool->mutex, NULL);
	pthread_cond_init(&thpool->cond, NULL);
	thpool->run = 1;
	thpool->timer = timer;

	for (i = 0; i < num; i++)
	{
		if (pthread_create(&thpool->tid[i], NULL, ThreadFunc, thpool) != 0)
		{
			ThreadPoolDestroy(thpool);
			printf("Create thread for thread pool failed\n");
			return NULL;
		}
		else
		{
			printf("++++++++Create thread id:%u\n", thpool->tid[i]);
		}
		thpool->thread_num++;
	}

	return thpool;
}

static int __cmp_id(util_rbtree_node_t *node, void *args)
{
	VirtualNode *vir;
	NewTimer *newtimer;

	newtimer = (NewTimer *)args;

	vir = (VirtualNode *)node->data;
	if (vir != newtimer->timer || vir->id != newtimer->id)
	{
		return -1;
	}

	return 0;
}

static util_rbtree_node_t *rbtree_find_mid_travel(util_rbtree_node_t *node, util_rbtree_node_t *sentinel,
												  int (*opera)(util_rbtree_node_t *, void *), void *args)
{
	util_rbtree_node_t *vnode;

	if (node->left != sentinel)
	{
		if ((vnode = rbtree_find_mid_travel(node->left, sentinel, opera, args)) != NULL)
		{
			return vnode;
		}
	}
	if (!opera(node, args))
	{
		return node;
	}
	if (node->right != sentinel)
	{
		if ((vnode = rbtree_find_mid_travel(node->right, sentinel, opera, args)) != NULL)
		{
			return vnode;
		}
	}

	return NULL;
}

static util_rbtree_node_t *util_find_rbtree_mid_travel(util_rbtree_t *rbtree,
													   int (*opera)(util_rbtree_node_t *, void *), void *args)
{
	if ((rbtree != NULL) && !util_rbtree_isempty(rbtree))
	{
		return rbtree_find_mid_travel(rbtree->root, _NULL(rbtree), opera, args);
	}

	return NULL;
}

static void *UtilRunTimer(void *vtimer)
{
	Timer *timer = (Timer *)vtimer;
	fd_set rfds;
	int retval = 0;

	struct timespec time1;
	struct timeval tv, cur, start;

	long long remain;
	VirtualNode *vir;
	util_rbtree_node_t *rbnode;
	util_rbtree_t *rbtree;

	if (!timer)
	{
		return NULL;
	}
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	while (timer->run)
	{
		FD_ZERO(&rfds);
		FD_SET(timer->fd[0], &rfds);

		// gettimeofday(&start, NULL);
		clock_gettime(CLOCK_MONOTONIC, &time1);
		start.tv_sec = time1.tv_sec;
		start.tv_usec = time1.tv_nsec / 1000;
		retval = select(timer->fd[0] + 1, &rfds, NULL, NULL, (tv.tv_sec == 0 && tv.tv_usec == 0) ? NULL : &tv);
		if (retval == -1)
		{
			if (errno == EINTR)
			{
				// gettimeofday(&cur, NULL);
				clock_gettime(CLOCK_MONOTONIC, &time1);
				cur.tv_sec = time1.tv_sec;
				cur.tv_usec = time1.tv_nsec / 1000;

				if (tv.tv_sec != 0)
				{
					tv.tv_sec -= cur.tv_sec - start.tv_sec;
				}
				if (tv.tv_usec != 0)
				{
					tv.tv_usec -= cur.tv_usec - start.tv_usec;
				}
				continue;
			}
			return NULL;
		}
		else
		{
			if (retval > 0)
			{
				char buf[2];
				recv(timer->fd[0], buf, 2, 0);
			}

			vir = NULL;
			do
			{
				rbtree = &timer->vnode_tree;
				
				pthread_mutex_lock(&timer->tree_mutex);
				if (!(rbnode = util_rbtree_min(rbtree)))
				{
					pthread_mutex_unlock(&timer->tree_mutex);
					tv.tv_sec = 0;
					tv.tv_usec = 0;
					vir = NULL;
					break;
				}
				// gettimeofday(&cur, NULL);
				clock_gettime(CLOCK_MONOTONIC, &time1);
				cur.tv_sec = time1.tv_sec;
				cur.tv_usec = time1.tv_nsec / 1000;
				vir = rbnode->data;
				remain = vir->val - (cur.tv_sec * 1000 + cur.tv_usec / 1000);
				if (remain <= 0)
				{
					util_rbtree_delete(rbtree, rbnode);
					if (0 != vir->repeat)
					{
						vir->val = cur.tv_sec * 1000 + cur.tv_usec / 1000 + vir->interval;
						rbnode->key = vir->val;
						util_rbtree_insert(&timer->vnode_tree, rbnode);
					}
					pthread_mutex_unlock(&timer->tree_mutex);
					if (g_thread_pool)
					{
						AddTask(g_thread_pool, (base_queue_t *)vir);
					}
					else
					{
						if (vir->handler)
						{
							vir->handler(vir->para, vir->tmpbuf, vir->cmd);
						}
						if (0 == vir->repeat)
						{
							/* recycle to pool queue */
							UtilRemoveVir(timer, vir);
						}
					}
					rbnode = NULL;
				}
				else
				{
					pthread_mutex_unlock(&timer->tree_mutex);
					break;
				}
			} while (!rbnode);

			if (vir)
			{
				tv.tv_sec = remain / 1000;
				tv.tv_usec = (remain % 1000) * 1000;
			}
		}
	}
	pthread_exit(NULL);
}

void *UtilTimerInit(int thread_num)
{
	if (g_timer != NULL)
	{
		return NULL;
	}

	Timer *timer;
	timer = (Timer *)malloc(sizeof(Timer));
	if (!timer)
		return NULL;
	memset(timer, 0, sizeof(Timer));
	timer->run = 1;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, timer->fd) != 0)
	{
		goto error;
	}
	util_rbtree_init(&timer->vnode_tree, util_rbtree_insert_value);
	base_queue_init(&timer->cache_queue);

	if (pthread_create(&timer->id, NULL, UtilRunTimer, timer) < 0)
	{
		UtilClose(timer);
		goto error;
	}
	pthread_mutex_init(&timer->queue_mutex, NULL);
	pthread_mutex_init(&timer->tree_mutex, NULL);

	g_thread_pool = ThreadPoolInit(timer, thread_num);

	g_timer = timer;
	return timer;

error:
	free(timer);
	return NULL;
}

static void UtilClose(Timer *timer)
{
	close(timer->fd[0]);
	close(timer->fd[1]);
}

static void UtilTriggerTimer(Timer *timer)
{
	send(timer->fd[1], " ", 1, 0);
}

void UtilTimerStop(NewTimer *args)
{
	util_rbtree_node_t *rbnode;
	VirtualNode *vir;
	Timer *timer = getTimerhandel();
	if (!args->timer || args->id <= 0)
	{
		return;
	}
	
	pthread_mutex_lock(&timer->tree_mutex);
	rbnode = util_find_rbtree_mid_travel(&timer->vnode_tree, __cmp_id, args);
	if (rbnode)
	{
		util_rbtree_delete(&timer->vnode_tree, rbnode);
		vir = (VirtualNode *)rbnode->data;
		UtilRemoveVir(timer, vir);
	}
	pthread_mutex_unlock(&timer->tree_mutex);
}

int UtilTimerCheck(NewTimer args)
{
	util_rbtree_node_t *rbnode;
	int ret = -1;
	Timer *timer = getTimerhandel();
	if (!args.timer || args.id <= 0)
	{
		return ret;
	}
	
	pthread_mutex_lock(&timer->tree_mutex);
	rbnode = util_find_rbtree_mid_travel(&timer->vnode_tree, __cmp_id, &args);
	if (rbnode)
	{
		ret = 0;
	}
	pthread_mutex_unlock(&timer->tree_mutex);

	return ret;
}

void UtilTimerClean()
{
	util_rbtree_node_t *rbnode;
	util_rbtree_t *rbtree;
	base_queue_t *p, *p2;
	Timer *timer = getTimerhandel();
	timer->run = 0;
	UtilTriggerTimer(timer);
	pthread_join(timer->id, NULL);

	ThreadPoolDestroy(g_thread_pool);

	rbtree = &timer->vnode_tree;
	
	pthread_mutex_lock(&timer->tree_mutex);
	while ((rbnode = util_rbtree_min(rbtree)) != NULL)
	{
		util_rbtree_delete(rbtree, rbnode);
		UtilRemoveVir(timer, (VirtualNode *)(rbnode->data));
	}
	pthread_mutex_unlock(&timer->tree_mutex);

	pthread_mutex_lock(&timer->queue_mutex);
	p = base_queue_head(&timer->cache_queue);
	while (base_queue_sentinel(p) != &timer->cache_queue)
	{
		p2 = p;
		p = base_queue_next(p);
		base_queue_remove(p2);
		free(p2);
	}
	pthread_mutex_unlock(&timer->queue_mutex);

	pthread_mutex_destroy(&timer->tree_mutex);
	pthread_mutex_destroy(&timer->queue_mutex);
	UtilClose(timer);
	free(timer);
}

static void UtilRemoveVir(Timer *timer, VirtualNode *vir)
{
	if (!vir)
	{
		return;
	}
	if (vir->buf_len > TMPBUF_LEN)
	{
		free(vir->tmpbuf);
	}
	
	pthread_mutex_lock(&timer->queue_mutex);
	base_queue_insert_tail(&timer->cache_queue, &vir->node);
	pthread_mutex_unlock(&timer->queue_mutex);
}

int UtilTimerStart(NewTimer *newtimer, uint64_t ms, int cmd, void *args, size_t args_len,
				   int (*handler)(void *, void *, int), void *para, char repeat)
{
	VirtualNode *vir, *vir1;
	struct timeval tv;
	struct timespec time1;
	util_rbtree_t *rbtree;
	util_rbtree_node_t *rbnode, *rbnode1;
	base_queue_t *p;
	void *tmpbuf = NULL;
	Timer *timer = getTimerhandel();
	memset(newtimer, 0, sizeof(NewTimer));
	if (args && args_len > TMPBUF_LEN)
	{
		tmpbuf = malloc(args_len);
		if (!tmpbuf)
		{
#ifdef _DEBUG
			printf("Malloc args buffer failed\n");
#endif
			return -1;
		}
	}
	
	pthread_mutex_lock(&timer->queue_mutex);
	p = base_queue_head(&timer->cache_queue);
	if (base_queue_sentinel(p) != &timer->cache_queue)
	{
		base_queue_remove(p);
		pthread_mutex_unlock(&timer->queue_mutex);
		vir = (void *)p;
	}
	else
	{
		pthread_mutex_unlock(&timer->queue_mutex);
		vir = malloc(sizeof(VirtualNode) + sizeof(util_rbtree_node_t) + TMPBUF_LEN);
	}
	if (!vir)
	{
		if (tmpbuf)
		{
			free(tmpbuf);
		}
		return -2;
	}
	if (tmpbuf)
	{
		vir->tmpbuf = tmpbuf;
	}
	else
	{
		vir->tmpbuf = (void *)((char *)vir + sizeof(VirtualNode) + sizeof(util_rbtree_node_t));
	}
	vir->buf_len = args_len;
	// gettimeofday(&tv, NULL);
	clock_gettime(CLOCK_MONOTONIC, &time1);
	tv.tv_sec = time1.tv_sec;
	tv.tv_usec = time1.tv_nsec / 1000;

	vir->val = tv.tv_sec * 1000 + tv.tv_usec / 1000 + ms;
#ifdef _DEBUG
	printf("Add Timer cmd:%ld, set timer:%llu\n", cmd, ms);
#endif
	vir->handler = handler;
	vir->para = para;
	vir->cmd = cmd;
	vir->id = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	vir->interval = ms;
	vir->repeat = repeat;
	memcpy(vir->tmpbuf, args, args_len);

	rbnode = (void *)((char *)vir + sizeof(VirtualNode));
	rbnode->key = vir->val;
	rbnode->data = vir;

	rbtree = &timer->vnode_tree;
	
	pthread_mutex_lock(&timer->tree_mutex);
	rbnode1 = util_rbtree_min(rbtree);
	util_rbtree_insert(rbtree, rbnode);
	if (rbnode1)
	{
		vir1 = rbnode1->data;
		if (vir1->val > vir->val)
		{
			pthread_mutex_unlock(&timer->tree_mutex);
			UtilTriggerTimer(timer);
		}
		else
		{
			pthread_mutex_unlock(&timer->tree_mutex);
		}
	}
	else
	{
		pthread_mutex_unlock(&timer->tree_mutex);
		UtilTriggerTimer(timer);
	}
	newtimer->timer = vir;
	newtimer->id = vir->id;

	return 0;
}
