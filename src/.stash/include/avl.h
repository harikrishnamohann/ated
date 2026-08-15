#pragma once

#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "itypes.h"
#include "utils.h"

#ifndef AVL_TYPE
#define AVL_TYPE u32
#endif

typedef struct node {
    AVL_TYPE     val;
    struct node* parent;
    struct node* lst;
    struct node* rst;
    u32          height;
} Node;

typedef struct {
    Node* root;
    i32 (*cmp)(void* a, void* b);
} AvlTree;

AvlTree avl_init(i32 (*cmp)(void* a, void* b));
void avl_insert(AvlTree *avl, AVL_TYPE val);
AVL_TYPE avl_remove(AvlTree *avl, AVL_TYPE val);
Node* avl_search(Node* root, AVL_TYPE key);
void avl_free(AvlTree* avl);


#ifdef AVL_IMPLEMENTATION

AvlTree avl_init(i32 (*cmp)(void* a, void* b))
{
    return (AvlTree) {
        .root = NULL,
        .cmp = cmp,
    };
}

static inline i32 height(Node* p)
{
    if (p != NULL)
        return p->height;
    else
        return -1;
}

static inline i32 BF(Node* p)
{
    if (p != NULL) {
        return height(p->rst) - height(p->lst);
     } else {
        return 0;
    }
}

static inline void update_height(Node* p)
{
    if (p != NULL) {
        p->height = MAX(height(p->lst), height(p->rst)) + 1;
    }
}

static Node* create_node(AVL_TYPE val)
{
    Node* node = (Node*)malloc(sizeof(Node));
    assert(node != NULL);
    node->lst    = node->rst = NULL;
    node->val    = val;
    node->parent = NULL;
    node->height = 0;
    return node;
}

static Node* rotate_left(Node* pivot)
{
    Node* P = pivot;
    Node* T = pivot->lst;
    if (T == NULL) return NULL;
    Node* TR = T->rst;
    Node* R = P->parent;

    if (R) {
        if (R->lst == P)
            R->lst = T;
        else 
            R->rst = T;
    }
    T->parent = R;

    P->lst = TR;
    if (TR) {
        TR->parent = P;
    }

    T->rst = P;
    P->parent = T;

    update_height(P);
    update_height(T);
    return T;
}

static Node* rotate_right(Node* pivot)
{
    if (pivot == NULL) return NULL;
    Node* P = pivot;
    Node* T = P->rst;
    Node* TL = T->lst;
    Node* R = P->parent;

    if (R) {
        if (R->lst == P)
            R->lst = T;
        else 
            R->rst = T;
    }
    T->parent = R;

    P->rst = TL;
    if (TL) TL->parent = P;

    T->lst = P;
    P->parent = T;

    update_height(P);
    update_height(T);
    
    return T;
}

static void avl_self_balance(AvlTree* avl, Node* modified)
{
    debug("\n== balancing from %d ==\n", modified->val);
    for (Node* curr = modified; curr != NULL; curr = curr->parent) {
        update_height(curr);
        debug("BF[%d]:%d\n", curr->val, BF(curr));
        if (BF(curr) < -1) { // left unbalanced
            debug("left heavy. performing ");

            if (BF(curr->lst) <= 0) { // LL case
                debug("LL");
                curr = rotate_left(curr);
            } else if (BF(curr->lst) >= +1) { // LR case
                debug("LR");
                curr->lst = rotate_right(curr->lst);
                curr = rotate_left(curr);
            }
            debug(" rotation.\n");

            if (curr->parent == NULL) {
                debug("updating root to [%d]\n", curr->val);
                avl->root = curr;
            }

        } else if (BF(curr) > +1) { // right unbalanced
            debug("right heavy. performing ");

            if (BF(curr->rst) >= 0) { // RR case
                debug("RR");
                curr = rotate_right(curr);
            } else if (BF(curr->rst) <= -1) { // RL case
                debug("RL");
                curr->rst = rotate_left(curr->rst);
                curr = rotate_right(curr);
            }

            debug(" rotation.\n");
            if (curr->parent == NULL) {
                debug("updating root to [%d]\n", curr->val);
                avl->root = curr;
            }
        }
    }
}

void avl_insert(AvlTree *avl, AVL_TYPE val)
{
    Node* curr = avl->root;
    Node* new_node = create_node(val);
    if (curr == NULL) {
        avl->root = new_node;
        return;
    }

    Node* prev = curr->parent;
    while (curr != NULL) {
        prev = curr;   
        if (avl->cmp(&val, &curr->val) < 0) {
            curr = curr->lst;
        } else {
            curr = curr->rst;
        }
    }

    if (avl->cmp(&val, &prev->val) < 0) {
        prev->lst = new_node;
    } else {
        prev->rst = new_node;
    }
    new_node->parent = prev;
    
    avl_self_balance(avl, new_node->parent);
}

AVL_TYPE avl_remove(AvlTree *avl, AVL_TYPE val)
{
    Node* curr = avl->root;
    
    while (curr != NULL) {
        if (avl->cmp(&curr->val, &val) == 0) {
            break;
        }
        if (val < curr->val)
            curr = curr->lst;
        else
            curr = curr->rst;            
    }

    if (curr == NULL) {
        printf("val not found in the tree\n");
        return -999;
    }
    
    AVL_TYPE deleted_val = curr->val;

    if (curr->lst && curr->rst) {
        Node* deepest_child = curr->rst; // right child of target node
        while (deepest_child->lst != NULL) { // finding a child higher and closest to target node
            deepest_child = deepest_child->lst;
        }

        curr->val = deepest_child->val; // swapping value of target node and deepest child
        curr = deepest_child; // curr now points to deepest child
    }

    // determine child that will replace curr.
    Node *parent = curr->parent;
    Node* child = (curr->lst != NULL) ? curr->lst : curr->rst;

    // Now we got a leaf node (0) or 1 child situation.
    if (parent == NULL) { // root node
        avl->root = child;
        if (avl->root) avl->root->parent = NULL;
    } else { 
        // link parent to child
        if (parent->lst == curr)
            parent->lst = child;
        else
            parent->rst = child;

        if (child) child->parent = parent;
    }

    free(curr);

    if (parent) avl_self_balance(avl, parent);
    return deleted_val;
}

Node* avl_search(Node* root, AVL_TYPE key)
{
    if (root == NULL || root->val == key) return root;
    if (key > root->val)
        return avl_search(root->rst, key);
    else
        return avl_search(root->lst, key);
}

#define STK_TYPE Node*
#define STK_ERR NULL
#define STK_IMPLEMENTATION
#include "stack.h"

void avl_free(AvlTree* avl)
{
    Stack stk = stack_init();
    push(&stk, avl->root);
    while (!STK_ISEMPTY(&stk)) {
        Node* curr = pop(&stk);
        if (curr->lst) push(&stk, curr->lst);
        if (curr->rst) push(&stk, curr->rst);
        curr->height = curr->val = 0;
        curr->lst = curr->rst = NULL;
        free(curr);
    }
    stk_free(&stk);
    avl->root = NULL;
    avl->cmp = NULL;
}

static void _visualize(Node* root)
{
    if (root == NULL) return;
    printf("[%d", root->val);
    if (root->lst) printf(" L:");
    _visualize(root->lst);
    if (root->rst) printf(" R:");
    _visualize(root->rst);
    printf("]");
} 

void display(Node* root)
{
    if (root == NULL) {
        printf("[]");
    }
    _visualize(root);
    printf("\n");
}

#endif

// i32 cmp(void* val1, void* val2) {
//     i32 a = *(i32*)val1;
//     i32 b = *(i32*)val2;
//     return a - b;
// }

// i32 main()
// {
//     AvlTree avl = avl_init(cmp);
//     i32 vals[] = { 50, 30, 10, 60, 70, 5, 8, 65, 2, 1 };
//     u32 n = sizeof(vals) / sizeof(int);
//     for (u32 i = 0; i < n; i++) {
//         avl_insert(&avl, vals[i]);
//     }
    
//     display(avl.root);

//     Node* res = avl_search(avl.root, 65);
//     if (res) {
//         printf("result found %d\n", res->val);
//     } else 
//         printf("Search failed\n");
    
//     avl_free(&avl);
//     return 0;
// }

