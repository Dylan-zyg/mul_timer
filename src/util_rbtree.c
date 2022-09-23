#include "util_rbtree.h"

/*
 * The red-black tree code is utild on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */

static inline void util_rbtree_left_rotate(util_rbtree_node_t **root,
                                           util_rbtree_node_t *sentinel, util_rbtree_node_t *node);
static inline void util_rbtree_right_rotate(util_rbtree_node_t **root,
                                            util_rbtree_node_t *sentinel, util_rbtree_node_t *node);

void util_rbtree_insert(volatile util_rbtree_t *tree,
                        util_rbtree_node_t *node)
{
    util_rbtree_node_t **root, *temp, *sentinel;

    /* a binary tree insert */

    root = (util_rbtree_node_t **)&tree->root;
    sentinel = (util_rbtree_node_t *)_NULL(tree);

    if (*root == sentinel)
    {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        util_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && util_rbt_is_red(node->parent))
    {

        if (node->parent == node->parent->parent->left)
        {
            temp = node->parent->parent->right;

            if (util_rbt_is_red(temp))
            {
                util_rbt_black(node->parent);
                util_rbt_black(temp);
                util_rbt_red(node->parent->parent);
                node = node->parent->parent;
            }
            else
            {
                if (node == node->parent->right)
                {
                    node = node->parent;
                    util_rbtree_left_rotate(root, sentinel, node);
                }

                util_rbt_black(node->parent);
                util_rbt_red(node->parent->parent);
                util_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }
        }
        else
        {
            temp = node->parent->parent->left;

            if (util_rbt_is_red(temp))
            {
                util_rbt_black(node->parent);
                util_rbt_black(temp);
                util_rbt_red(node->parent->parent);
                node = node->parent->parent;
            }
            else
            {
                if (node == node->parent->left)
                {
                    node = node->parent;
                    util_rbtree_right_rotate(root, sentinel, node);
                }

                util_rbt_black(node->parent);
                util_rbt_red(node->parent->parent);
                util_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    util_rbt_black(*root);
}

void util_rbtree_insert_value(util_rbtree_node_t *temp, util_rbtree_node_t *node,
                              util_rbtree_node_t *sentinel)
{
    util_rbtree_node_t **p;

    for (;;)
    {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel)
        {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    util_rbt_red(node);
}

void util_rbtree_insert_timer_value(util_rbtree_node_t *temp, util_rbtree_node_t *node,
                                    util_rbtree_node_t *sentinel)
{
    util_rbtree_node_t **p;

    for (;;)
    {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */

        p = ((util_rbtree_key_int_t)(node->key - temp->key) < 0)
                ? &temp->left
                : &temp->right;

        if (*p == sentinel)
        {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    util_rbt_red(node);
}

void util_rbtree_delete(volatile util_rbtree_t *tree,
                        util_rbtree_node_t *node)
{
    uint32_t red;
    util_rbtree_node_t **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = (util_rbtree_node_t **)&tree->root;
    sentinel = (util_rbtree_node_t *)_NULL(tree);

    if (node->left == sentinel)
    {
        temp = node->right;
        subst = node;
    }
    else if (node->right == sentinel)
    {
        temp = node->left;
        subst = node;
    }
    else
    {
        subst = util_rbsubtree_min(node->right, sentinel);

        if (subst->left != sentinel)
        {
            temp = subst->left;
        }
        else
        {
            temp = subst->right;
        }
    }

    if (subst == *root)
    {
        *root = temp;
        util_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = util_rbt_is_red(subst);

    if (subst == subst->parent->left)
    {
        subst->parent->left = temp;
    }
    else
    {
        subst->parent->right = temp;
    }

    if (subst == node)
    {

        temp->parent = subst->parent;
    }
    else
    {

        if (subst->parent == node)
        {
            temp->parent = subst;
        }
        else
        {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        util_rbt_copy_color(subst, node);

        if (node == *root)
        {
            *root = subst;
        }
        else
        {
            if (node == node->parent->left)
            {
                node->parent->left = subst;
            }
            else
            {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel)
        {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel)
        {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red)
    {
        return;
    }

    /* a delete fixup */

    while (temp != *root && util_rbt_is_black(temp))
    {

        if (temp == temp->parent->left)
        {
            w = temp->parent->right;

            if (util_rbt_is_red(w))
            {
                util_rbt_black(w);
                util_rbt_red(temp->parent);
                util_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (util_rbt_is_black(w->left) && util_rbt_is_black(w->right))
            {
                util_rbt_red(w);
                temp = temp->parent;
            }
            else
            {
                if (util_rbt_is_black(w->right))
                {
                    util_rbt_black(w->left);
                    util_rbt_red(w);
                    util_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                util_rbt_copy_color(w, temp->parent);
                util_rbt_black(temp->parent);
                util_rbt_black(w->right);
                util_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
        else
        {
            w = temp->parent->left;

            if (util_rbt_is_red(w))
            {
                util_rbt_black(w);
                util_rbt_red(temp->parent);
                util_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (util_rbt_is_black(w->left) && util_rbt_is_black(w->right))
            {
                util_rbt_red(w);
                temp = temp->parent;
            }
            else
            {
                if (util_rbt_is_black(w->left))
                {
                    util_rbt_black(w->right);
                    util_rbt_red(w);
                    util_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                util_rbt_copy_color(w, temp->parent);
                util_rbt_black(temp->parent);
                util_rbt_black(w->left);
                util_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    util_rbt_black(temp);
}

static inline void
util_rbtree_left_rotate(util_rbtree_node_t **root, util_rbtree_node_t *sentinel,
                        util_rbtree_node_t *node)
{
    util_rbtree_node_t *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel)
    {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root)
    {
        *root = temp;
    }
    else if (node == node->parent->left)
    {
        node->parent->left = temp;
    }
    else
    {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}

static inline void
util_rbtree_right_rotate(util_rbtree_node_t **root, util_rbtree_node_t *sentinel,
                         util_rbtree_node_t *node)
{
    util_rbtree_node_t *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel)
    {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root)
    {
        *root = temp;
    }
    else if (node == node->parent->right)
    {
        node->parent->right = temp;
    }
    else
    {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}
