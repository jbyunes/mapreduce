/*
 * A multiset of words represented in a multi-level prefix tree (trie).
 *
 * Ex.: a, a, at, ban
 * () node, [] alpha_node
 * ( [a,2,([t,1,nil])] , [b,0,([a,0,([n,1,nil]))] )
 *
 * or
 *
 * |
 * a,2----b,0
 * |      |
 * t,1    a,0
 *        |
 *        n,1
 */

// Structures

struct node;

struct alpha_node {
  int c;        // the char
  int count;    // number of words ending with this letter
  struct node *one_letter_more; // if suffixes exist
};

struct node {
  int len; // number of letters at this level
  struct alpha_node *letters; // the letters (ASCII-ordered)
};

// Prototypes
struct alpha_node *is_in(int c,struct node *node);
struct alpha_node *insert_letter(int c,struct node *node);
void merge_trees(struct node *in,struct node *from);
void print_tree(struct node *root);
void insert_word(char *word,struct node *root);
void deallocate_tree(struct node *root);
