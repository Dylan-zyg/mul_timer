#ifndef _UTIL_RBTREE_H_INCLUDED_
#define _UTIL_RBTREE_H_INCLUDED_

#include <stdio.h>
#include <stdint.h>

typedef uint32_t util_rbtree_key_t;
typedef uint32_t util_rbtree_key_int_t;

typedef struct util_rbtree_node_s util_rbtree_node_t;

struct util_rbtree_node_s
{
	util_rbtree_key_t key;
	util_rbtree_node_t *left;
	util_rbtree_node_t *right;
	util_rbtree_node_t *parent;
	char color;
	void *data;
};

typedef struct util_rbtree_s util_rbtree_t;

typedef void (*util_rbtree_insert_pt)(util_rbtree_node_t *root,
									  util_rbtree_node_t *node, util_rbtree_node_t *sentinel);

struct util_rbtree_s
{
	util_rbtree_node_t *root;
	util_rbtree_node_t null;
	util_rbtree_insert_pt insert;
};

/* the NULL node of tree */
#define _NULL(rbtree) (&((rbtree)->null))

/* is the tree empty */
#define util_rbtree_isempty(rbtree) ((rbtree)->root == &(rbtree)->null)

void util_rbtree_insert(volatile util_rbtree_t *tree,
						util_rbtree_node_t *node);
void util_rbtree_delete(volatile util_rbtree_t *tree,
						util_rbtree_node_t *node);
void util_rbtree_insert_value(util_rbtree_node_t *root, util_rbtree_node_t *node,
							  util_rbtree_node_t *sentinel);
void util_rbtreee_insert_timer_value(util_rbtree_node_t *root,
									 util_rbtree_node_t *node, util_rbtree_node_t *sentinel);

#define util_rbt_red(node) ((node)->color = 1)
#define util_rbt_black(node) ((node)->color = 0)
#define util_rbt_is_red(node) ((node)->color)
#define util_rbt_is_black(node) (!util_rbt_is_red(node))
#define util_rbt_copy_color(n1, n2) (n1->color = n2->color)

/* a sentinel must be black */
#define util_rbtree_init(tree, i) \
	(tree)->root = _NULL(tree);   \
	util_rbt_black(_NULL(tree));  \
	(tree)->insert = i

static inline util_rbtree_node_t *
util_rbsubtree_min(util_rbtree_node_t *node, util_rbtree_node_t *sentinel)
{
	if (node == sentinel)
		return NULL;
	while (node->left != sentinel)
	{
		node = node->left;
	}

	return node;
}

static inline util_rbtree_node_t *
util_rbsubtree_max(util_rbtree_node_t *node, util_rbtree_node_t *sentinel)
{
	if (node == sentinel)
		return NULL;
	while (node->left != sentinel)
	{
		node = node->left;
	}

	return node;
}

/*
 * find the min node of tree
 * return NULL is tree is empty
 */
#define util_rbtree_min(rbtree) util_rbsubtree_min((rbtree)->root, &(rbtree)->null)

/*
 * find the max node of tree
 * return NULL is tree is empty
 */
#define util_rbtree_max(rbtree) util_rbsubtree_max((rbtree)->root, &(rbtree)->null)

#endif /* _UTIL_RBTREE_H_INCLUDED_ */
