/*
 * Utilitary functions on tries.
 *
 * Warning: Some may fail in non-trapped bad allocation.
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <treelib.h>

/*
 * Searching for a letter in a given tree node.
 *
 * inputs: c, the char to find; node, the node to search in.
 * output: NULL if c not in node, the corresponding alphabetic entry if in
 *
 * TODO: may be usefully accelerated with dichotomy
 */
struct alpha_node *is_in(int c,struct node *node) {
  for (int i=0; i<node->len; i++) {
    if (c==node->letters[i].c) return node->letters+i;
  }
  return NULL;
}

/*
 * Initialize an alphabetic node for a given letter.
 *
 * inputs: c, the character; n, the node
 * output: none
 *
 * TODO: catch allocation problems
 */
static inline void init_alpha_node(int c,struct alpha_node *n) {
  n->c = c;
  n->count = 0;
  n->one_letter_more = malloc(sizeof(struct node));
  if (n->one_letter_more==NULL) {
    fprintf(stderr,"[ERR] alloc problem\n");
    exit(1);
  }
  n->one_letter_more->len = 0;
  n->one_letter_more->letters = NULL;
}

/*
 * Insert a new alphabetic node in a given node (provided the letter does
 * not currently appears in.
 *
 * inputs: c, the char to insert; node, the node to insert in.
 * output: the newly created alphabetic node representing the letter
 */
struct alpha_node *insert_letter(int c,struct node *node) {
  int i;
  
  node->letters = realloc(node->letters,(node->len+1)*sizeof(struct alpha_node));
  if (node->letters==NULL) {
      fprintf(stderr,"[ERR] alloc problem\n");
    exit(1);
  }

  if (node->len>0) { // there was previously something
    for (i=0; i<node->len && c>node->letters[i].c; i++); // find its place
    if (i<node->len) {
      for (int j=node->len; j>i; j--)
        node->letters[j] = node->letters[j-1];
    }
  } else { // c is the first letter at this level
    i = 0;
  }
  node->len++; // one more
  init_alpha_node(c,node->letters+i);
  return node->letters+i;
}

/*
 * In place merging of 2 trees. Parallel recursive descent.
 *
 * Inputs: in, the tree to merge in; from, the tree to be added
 * Output: none
 */
void merge_trees(struct node *in,struct node *from) {
  if (from->len>0) { // something to add ?
    for (int i=0; i<from->len; i++) { // add every alphabetic node
      struct alpha_node *n;
      if ( (n=is_in(from->letters[i].c,in))==NULL) { // if not in, create new
        n = insert_letter(from->letters[i].c,in);
      }
      n->count += from->letters[i].count; // merge counts
      merge_trees(n->one_letter_more,from->letters[i].one_letter_more);
    }
  }
}

/*
 * Print the list of words on screen. (Private implementation).
 *
 * inputs: prefix, string prefix for letters; root, root of the subtree
 * output: none
 */
static void _print_tree(char *prefix,struct node *root) {
  if (root->len>0) {
    int l = strlen(prefix);  
    char *new_prefix = malloc(l+2); // create a new temporary prefix of length+1
    if (new_prefix==NULL) {
      fprintf(stderr,"[ERR] alloc problem\n");
      exit(1);
    }
    strcpy(new_prefix,prefix);
    new_prefix[l+1] = '\0';
    for (int i=0; i<root->len; i++) { // parse all letters
      new_prefix[l] = (char)(root->letters[i].c);
      if (root->letters[i].count!=0) // if ends a word, print it
        printf("%s=%d\n",new_prefix,root->letters[i].count);
      _print_tree(new_prefix,root->letters[i].one_letter_more);
    }
    free(new_prefix);
  }
}

/*
 * Prints the list of words of a given tree on screen. (Public stub).
 *
 * inputs: the root of the tree
 * output: none
 */
void print_tree(struct node *root) {
  _print_tree("",root);
}

/*
 * Insert a word to a given tree. (Iterative)
 *
 * inputs: word, the word to add; root, the tree
 * output: none
 */
void insert_word(char *word,struct node *root) {
  int c;
  struct alpha_node *n;
  while ( (c=(*word++))!=0 ) {
    if ( (n=is_in(c,root))==NULL ) {
      n = insert_letter(c,root);
    }
    if (!*word) {
      n->count++;
      return;
    }
    root = n->one_letter_more;
  }
}

/*
 * Tree deallocation
 */
void deallocate_tree(struct node *root) {
  if (root==NULL) return;
  for (int i=0; i<root->len; i++) {
    deallocate_tree(root->letters[i].one_letter_more);
    free(root->letters[i].one_letter_more);
  }
  free(root->letters);
}

