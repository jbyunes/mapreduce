#define TIMED // time the results
//#define MYDEBUG(s,...) printf("[DBG]" s,__VA_ARGS__)
#define MYDEBUG(s,...)

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <treelib.h>
#ifdef TIMED
#include <sys/time.h>
#endif // TIMED

#define ISALPHA(c) isalpha(c)

/*
 * A chunk contains:
 * - the portion of the file a single thread should parse
 * - the result tree
 */
struct chunk {
  long start;     // a thread should start its parsing from that position
  long len;       // the length of the part the thread must parse
  FILE *file;     // the file, thre thread is working on
  int word_count; // the number of words a thread found
  struct node root; // the multi-set of words a thread found
};

/*
 * This is used to properly cut out jobs.
 * A naive cut may be in the middle of a word, so this function jumps to 
 * the beginning of the next word in the file from a given position.
 */
static void find_next_starting_word(FILE *f) {
  int c;
  while ( ((c=fgetc(f))!=EOF) && (ISALPHA(c)) ); // jump over alphabetics
  if (c==EOF) return;
  while ( ((c=fgetc(f)) != EOF) && (!ISALPHA(c)) ); // jump over non alphabetic
  if (c!=EOF) ungetc(c,f); // one step ahead, get back
}

/*
 * Describes the task a thread must do.
 * inputs: arg, a chunk
 * output: via arg, a word count and a word multi-set.
 *
 * Thread:
 * - parse its chunk, getting out each word (a word is a non empty list
 *   of alphetic chars - every alphabetic char is converted to its lower
 *   case).
 * Note that the chunk exactly starts at the beginning of a word...
 *
 * TODO: we may also try to descend the tree while progressing in a word...
 * Not sure this will seriously optimize the time (need test).
 */
void *task(void *arg) {
  struct chunk *chunk = arg;
  int count = 0;
  int in_word = 1;
  int c;
  char *word = malloc(1);
  int l;
  *word = '\0';
  chunk->word_count = 0;
  if (chunk->len==0) return chunk;
  while ( (c=fgetc(chunk->file)) != EOF && count<chunk->len ) {
    if (in_word) { // already in the middle of a word
      if (ISALPHA(c)) { // alphabetic?
        l = strlen(word);
        word = realloc(word,strlen(word)+2); // one more char
        word[l] = tolower(c);
        word[l+1] = '\0';
      } else { // non alphabetic, so end of a word
        in_word = 0; // no more in a word
        chunk->word_count++; // one more word
        insert_word(word, &(chunk->root)); // insert the word in the set
        free(word);
        word = malloc(1); // new empty word
        *word = '\0';
      }
    } else { // not in a word
      if (ISALPHA(c)) { // alphabetic?
        in_word = 1; // in a word now
        word = realloc(word,2); // one more char
        word[0] = tolower(c);
        word[1] = '\0';
      } else { // nothing to do here, not in w word and not an alphabetic...
      }
    }
    count++;
  }
  if (c==EOF && in_word) { // one new word on end of file...
    chunk->word_count++;
    insert_word(word, &(chunk->root)); // insert the word in the set
  }
  free(word);
  return chunk;
}

/*
 * Determines the number of threads.
 * Distributes the computations to them.
 * Collect the results.
 *
 * Warning, memory and files are not explicitly freed and closed...
 * System will gracefully clean everything at termination.
 */
int main(int argc,char *argv[]) {
#ifdef TIMED
  struct timeval time_start, time_end;
#endif // TIMED
  char *name; // Filename
  int thread_count; // Total number of threads (dynamically adjusted)
  struct stat stat_buf; // File properties
  struct chunk *chunks; // Chunks
  struct chunk *status; // Pointer to a thread result
  pthread_t *threads;   // Threads
  long chunk_size; // Approximated chunk size
  int total_count; // Number of found words
  struct node root; // Result tree

  // Decode args
  if (argc!=3) {
    fprintf(stderr,"usage: %s file N\n",argv[0]);
    exit(EXIT_FAILURE);
  }
  name = argv[1];
  MYDEBUG("%s: file is %s\n",argv[0],name);
  if (sscanf(argv[2],"%d",&thread_count)!=1) {
    fprintf(stderr,"usage: %s file N\n",argv[0]);
    exit(EXIT_FAILURE);
  }
  if (thread_count<1) {
    fprintf(stderr,"%s: bad number of threads must be >0\n",argv[0]);
    exit(EXIT_FAILURE);
  }
  stat(name,&stat_buf);
  chunks = calloc(thread_count,sizeof(struct chunk));

  // Create chunks
  // First open the file (each thread has its own open file
  MYDEBUG("%s: determining real number of threads (current %d)\n",argv[0],thread_count);
  for (int i=0; i<thread_count; i++) {
    chunks[i].file = fopen(name,"r");
    if (chunks[i].file==NULL) {
      thread_count = i; // no more than i concurrent open files
      break;
    }
  }
  if (thread_count<1) {
    fprintf(stderr,"[ERROR] %s: problem opening \"%s\" even once\n",argv[0],name);
    exit(EXIT_FAILURE);
  }
  threads = calloc(thread_count,sizeof(pthread_t));
  fprintf(stderr,"[LOG] %s: real number of threads is %d\n",argv[0],thread_count);

  // Basic chunk carateristics
  MYDEBUG("%s: computing %d chunks\n",argv[0],thread_count);
  chunk_size = stat_buf.st_size/thread_count;
  chunk_size = chunk_size<1?1:chunk_size;
  MYDEBUG("%s: chunk size is %ld\n",argv[0],chunk_size);

  // Adjust chunks caracteristics
  // Each starts at a word boundary
  // This may decrease the number of total threads
  MYDEBUG("%s: computing chunk #%d\n",argv[0],0);
  chunks[0].start = 0;
  fseek(chunks[0].file,chunks[0].start+chunk_size,SEEK_SET);
  find_next_starting_word(chunks[0].file); // find a word boundary
  chunks[0].len = ftell(chunks[0].file)-chunks[0].start;
  fseek(chunks[0].file,chunks[0].start,SEEK_SET); // reset at start
  if (chunks[0].start+chunks[0].len>=stat_buf.st_size) {
    thread_count = 1;
    fprintf(stderr,"[LOG] %s: reducing to %d threads\n",argv[0],thread_count);
    chunks[0].len = stat_buf.st_size-chunks[0].start;
  }
  for (int i=1; i<thread_count; i++) {
    MYDEBUG("%s: computing chunk #%d\n",argv[0],i);
    chunks[i].start = chunks[i-1].start+chunks[i-1].len;
    if (chunks[i].start>=stat_buf.st_size) {
      thread_count = i;
      fprintf(stderr,"[LOG] %s: reducing to %d threads\n",argv[0],thread_count);
    }
    fseek(chunks[i].file,chunks[i].start+chunk_size,SEEK_SET);
    find_next_starting_word(chunks[i].file); // find a word boundary
    chunks[i].len = ftell(chunks[i].file)-chunks[i].start;
    fseek(chunks[i].file,chunks[i].start,SEEK_SET); // reset at start
    if (chunks[i].start+chunks[i].len>=stat_buf.st_size) {
      chunks[i].len = stat_buf.st_size-chunks[i].start;
    }
  }
  
  for (int i=0; i<thread_count; i++) {
    MYDEBUG("%s: chunk %ld %ld\n",argv[0],chunks[i].start,chunks[i].len);
  }

  // MAP: Start the threads
#ifdef TIMED
  gettimeofday(&time_start,NULL);
#endif //TIMED
  fprintf(stderr,"[LOG] %s: starting creating %d thread(s)\n",argv[0],thread_count);
    for (int i=0; i<thread_count; i++) {
      pthread_create(threads+i,NULL,task,chunks+i);
    }
  
  MYDEBUG("%s: waiting results\n",argv[0]);

  // Init result
  root.len = 0;
  root.letters = NULL;
  total_count = 0;

  //REDUCE: Accumulate results
  for (int i=0; i<thread_count; i++) {
    pthread_join(threads[i],(void **)&status);
    total_count += status->word_count;
    MYDEBUG("%s: a thread found %d words (%ld %ld)\n",argv[0],status->word_count,status->start,status->len);
    merge_trees(&root,&(status->root));
  }
  fprintf(stderr,"[LOG] %s: Found %d words\n",argv[0],total_count);

#ifdef TIMED
  gettimeofday(&time_end,NULL);
  if (time_start.tv_usec>time_end.tv_usec) {
    time_end.tv_usec += 1000000;
    time_end.tv_sec--;
  }
  fprintf(stderr,"[LOG] %s: ELAPSED %4ld,%06d\n",
          argv[0],
          time_end.tv_sec-time_start.tv_sec,
          time_end.tv_usec-time_start.tv_usec);
#endif // TIMED

  // Show results
  print_tree(&root);
  
  exit(EXIT_SUCCESS);
}
