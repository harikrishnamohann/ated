#include <stdio.h>
#include <malloc.h>
#include "itypes.h"
#include <assert.h>

typedef struct node {
    i32 val;
    struct node* lst;
    struct node* rst;
} Node;

Node* create_node(i32 val)
{
    Node* node = (Node*)malloc(sizeof(Node));
    assert(node != NULL);
    node->lst = node->rst = NULL;
    node->val = val;
    return node;
}

void insert_node(Node** root, i32 val)
{
    Node* curr = *root, *prev = NULL;
    assert(curr != NULL);
    Node* new_node = create_node(val);

    while (curr != NULL) {
        prev = curr;   
        if (val < curr->val) {
            curr = curr->lst;
        } else {
            curr = curr->rst;
        }
    }

    if (val < prev->val) {
        prev->lst = new_node;
    } else {
        prev->rst = new_node;
    }
}

i32 remove_node(Node** root, i32 val)
{
    Node* curr = *root;
    Node* parent = NULL;
    
    while (curr != NULL) {
        if (curr->val == val) {
            break;
        }
        parent = curr;
        if (val < curr->val) {
            curr = curr->lst;
        } else {
            curr = curr->rst;            
        }
    }

    if (curr == NULL) {
        printf("val not found in the tree\n");
        return -999;
    }
    
    i32 result = curr->val;

    if (curr->lst != NULL && curr->rst != NULL) {
        Node* min_parent = curr;
        Node* min_node = curr->rst;
        while (min_node->lst != NULL) {
            min_parent = min_node;
            min_node = min_node->lst;
        }

        curr->val = min_node->val;
        curr = min_node;
        parent = min_parent;
    }

    if (parent == NULL) {
        *root = (curr->lst != NULL) ? curr->lst : curr->rst;
    } else {
        Node** child;
        if (parent->lst == curr) {
            child = &parent->lst;
        } else {
            child = &parent->rst;
        }

        if (curr->lst) {
            *child = curr->lst;
        } else if (curr->rst) {
            *child = curr->rst;
        } else { // leaf node
            *child = NULL;
        }
    }
    free(curr);

    return result;
}

void _visualize(Node* root)
{
    if (root == NULL) return;
    printf("[%d", root->val);
    if (root->lst) printf(" L:");
    _visualize(root->lst);
    if (root->rst) printf(" R:");
    _visualize(root->rst);
    printf("]");
} 

void debug(Node* root) {
    _visualize(root);
    printf("\n");
}

i32 main()
{
    Node* root = create_node(10);
    insert_node(&root, 8);
    insert_node(&root, 18);
    insert_node(&root, 17);
    insert_node(&root, 2);
    insert_node(&root, 9);
    insert_node(&root, 20);
    remove_node(&root, 2);
    remove_node(&root, 10);
    insert_node(&root, 3);
    debug(root);
    return 0;
}
