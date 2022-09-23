#include "util_queue.h"

/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */

base_queue_t *
base_queue_middle(base_queue_t *queue)
{
	base_queue_t *middle, *next;

	middle = base_queue_head(queue);

	if (middle == base_queue_last(queue))
	{
		return middle;
	}

	next = base_queue_head(queue);

	for (;;)
	{
		middle = base_queue_next(middle);

		next = base_queue_next(next);

		if (next == base_queue_last(queue))
		{
			return middle;
		}

		next = base_queue_next(next);

		if (next == base_queue_last(queue))
		{
			return middle;
		}
	}
}

/* the stable insertion sort */

void base_queue_sort(base_queue_t *queue,
					 intptr_t (*cmp)(const base_queue_t *, const base_queue_t *))
{
	base_queue_t *q, *prev, *next;

	q = base_queue_head(queue);

	if (q == base_queue_last(queue))
	{
		return;
	}

	for (q = base_queue_next(q); q != base_queue_sentinel(queue); q = next)
	{
		prev = base_queue_prev(q);
		next = base_queue_next(q);

		base_queue_remove(q);

		do
		{
			if (cmp(prev, q) <= 0)
			{
				break;
			}

			prev = base_queue_prev(prev);
		} while (prev != base_queue_sentinel(queue));

		base_queue_insert_after(prev, q);
	}
}

#if 0
typedef struct connect_ {
	int id;

	base_queue_t queue;
} connect_t;

int main(int argc ,char *argv[])
{
	base_queue_t con_queue, *p;
	connect_t    conn1, conn2, conn3, *c;

	conn1.id = 1;
	conn2.id = 3;
	conn3.id = 2;

	base_queue_init(&con_queue);
	base_queue_insert_tail(&con_queue, &conn1.queue);
	base_queue_insert_tail(&con_queue, &conn2.queue);
	base_queue_insert_tail(&con_queue, &conn3.queue);	

	p = base_queue_head(&con_queue);
	printf("offset:%d, sizeof(int):%d\n", offsetof(connect_t, queue), sizeof(int));


	while (base_queue_sentinel(p) != &con_queue) {
		c = base_queue_data(p, connect_t, queue);
		printf("data:%d\n", c->id);
		p = base_queue_next(p);
	}

	return 0;
}

#endif
