#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>

#ifndef _BASE_QUEUE_H_INCLUDED_
#define _BASE_QUEUE_H_INCLUDED_

typedef struct base_queue_s base_queue_t;

struct base_queue_s
{
	base_queue_t *prev;
	base_queue_t *next;
};

#define base_queue_init(q) \
	(q)->prev = q;         \
	(q)->next = q

#define base_queue_empty(h) \
	(h == (h)->prev)

#define base_queue_insert_head(h, x) \
	(x)->next = (h)->next;           \
	(x)->next->prev = x;             \
	(x)->prev = h;                   \
	(h)->next = x

#define base_queue_insert_after base_queue_insert_head

#define base_queue_insert_tail(h, x) \
	(x)->prev = (h)->prev;           \
	(x)->prev->next = x;             \
	(x)->next = h;                   \
	(h)->prev = x

#define base_queue_head(h) \
	(h)->next

#define base_queue_last(h) \
	(h)->prev

#define base_queue_sentinel(h) \
	(h)

#define base_queue_next(q) \
	(q)->next

#define base_queue_prev(q) \
	(q)->prev

#ifdef BASE_DEBUG

#define base_queue_remove(x)     \
	(x)->next->prev = (x)->prev; \
	(x)->prev->next = (x)->next; \
	(x)->prev = NULL;            \
	(x)->next = NULL

#else

#define base_queue_remove(x)     \
	(x)->next->prev = (x)->prev; \
	(x)->prev->next = (x)->next

#endif

#define base_queue_split(h, q, n) \
	(n)->prev = (h)->prev;        \
	(n)->prev->next = n;          \
	(n)->next = q;                \
	(h)->prev = (q)->prev;        \
	(h)->prev->next = h;          \
	(q)->prev = n

#define base_queue_add(h, n)     \
	(h)->prev->next = (n)->next; \
	(n)->next->prev = (h)->prev; \
	(h)->prev = (n)->prev;       \
	(h)->prev->next = h

#define base_queue_data(q, type, link) \
	(type *)((u_char *)q - offsetof(type, link))

base_queue_t *base_queue_middle(base_queue_t *queue);
void base_queue_sort(base_queue_t *queue,
					 intptr_t (*cmp)(const base_queue_t *, const base_queue_t *));

#endif /* _BASE_QUEUE_H_INCLUDED_ */
