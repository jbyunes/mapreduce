#define TIMED // time the results
#define MYDEBUG(s,...) printf("[DBG] %s:" s,cmd_name,__VA_ARGS__)
//#define MYDEBUG(s,...)

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

#define IS_ALPHA(c) isalpha(c)

static char *cmd_name;

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
  int threaded;
};

/*
 * This is used to properly cut out jobs.
 * A naive cut may be in the middle of a word, so this function jumps to 
 * the beginning of the next word in the file from a given position.
 */
static void find_next_starting_word(FILE *f) {
  int c;
  while ( ((c=fgetc(f))!=EOF) && (IS_ALPHA(c)) ); // jump over alphabetics
  if (c==EOF) return;
  while ( ((c=fgetc(f)) != EOF) && (!IS_ALPHA(c)) ); // jump over non alphabetic
  if (c!=EOF) ungetc(c,f); // one step ahead, get back
}

/*
 * Prepare chunk mapping. Each chunk is adjusted to a word boundary, starting
 * at the beginning of a word or beginning of file, ending at the beginning
 * of a word of end of file.
 *
 * inputs: chunks, the original chunks; thread_count;
 *         stat_buf, file properties; chunk_size, naive chunk_size estimated
 *         from number of thread and filz size; argv0, command name
 * output; adujsted chunks and thread_count
 */
void prepare_chunks(struct chunk *chunks,int *thread_count,struct stat stat_buf,
                    int chunk_size,char *argv0) {
  // First chunk starts at 0
  MYDEBUG("computing chunk #%d\n",0);
  chunks[0].start = 0;
  fseek(chunks[0].file,chunks[0].start+chunk_size,SEEK_SET);
  find_next_starting_word(chunks[0].file); // find a word boundary
  chunks[0].len = ftell(chunks[0].file)-chunks[0].start;
  fseek(chunks[0].file,chunks[0].start,SEEK_SET); // reset to start
  if (chunks[0].start+chunks[0].len>=stat_buf.st_size) {
    *thread_count = 1;
    fprintf(stderr,"[LOG] %s: reducing to %d threads\n",cmd_name,*thread_count);
    chunks[0].len = stat_buf.st_size-chunks[0].start;
  }
  
  for (int i=1; i<*thread_count; i++) {
    MYDEBUG("computing chunk #%d\n",i);
    chunks[i].start = chunks[i-1].start+chunks[i-1].len;
    if (chunks[i].start>=stat_buf.st_size) {
      *thread_count = i;
      fprintf(stderr,"[LOG] %s: reducing to %d threads\n",cmd_name,*thread_count);
    }
    fseek(chunks[i].file,chunks[i].start+chunk_size,SEEK_SET);
    find_next_starting_word(chunks[i].file); // find a word boundary
    chunks[i].len = ftell(chunks[i].file)-chunks[i].start;
    fseek(chunks[i].file,chunks[i].start,SEEK_SET); // reset at start
    if (chunks[i].start+chunks[i].len>=stat_buf.st_size) {
      chunks[i].len = stat_buf.st_size-chunks[i].start;
    }
  }
  // In case of "debug" messages, should be optimized at compilation
  for (int i=0; i<*thread_count; i++) {
    MYDEBUG("chunk %ld %ld\n",chunks[i].start,chunks[i].len);
  }
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
      if (IS_ALPHA(c)) { // alphabetic?
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
      if (IS_ALPHA(c)) { // alphabetic?
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
  fclose(chunk->file); // gracefully close file
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
  cmd_name = argv[0]; // for DEBUG
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
    fprintf(stderr,"usage: %s file N\n",cmd_name);
    exit(EXIT_FAILURE);
  }
  name = argv[1];
  MYDEBUG("file is %s\n",name);
  if (sscanf(argv[2],"%d",&thread_count)!=1) {
    fprintf(stderr,"usage: %s file N\n",cmd_name);
    exit(EXIT_FAILURE);
  }
  if (thread_count<1) {
    fprintf(stderr,"[ERR] %s: bad number of threads must be >0\n",cmd_name);
    exit(EXIT_FAILURE);
  }
  stat(name,&stat_buf);
  chunks = calloc(thread_count,sizeof(struct chunk));

  // Create chunks
  // First open the file (each thread has its own open file
  MYDEBUG("determining real number of threads (current %d)\n",thread_count);
  for (int i=0; i<thread_count; i++) {
    chunks[i].file = fopen(name,"r");
    if (chunks[i].file==NULL) {
      thread_count = i; // no more than i concurrent open files
      break;
    }
  }
  if (thread_count<1) {
    fprintf(stderr,"[ERR] %s: problem opening \"%s\" even once\n",cmd_name,name);
    exit(EXIT_FAILURE);
  }
  threads = calloc(thread_count,sizeof(pthread_t));
  fprintf(stderr,"[LOG] %s: real number of threads is %d\n",cmd_name,thread_count);

  // Basic chunk charateristics
  MYDEBUG("computing %d chunks\n",thread_count);
  chunk_size = stat_buf.st_size/thread_count;
  chunk_size = chunk_size<1?1:chunk_size;
  MYDEBUG("chunk size is %ld\n",chunk_size);

  // Adjust chunks characteristics
  prepare_chunks(chunks,&thread_count,stat_buf,chunk_size,cmd_name);

  // MAP: Start the threads, in case of failure main thread will compute
#ifdef TIMED
  gettimeofday(&time_start,NULL);
#endif //TIMED
  fprintf(stderr,"[LOG] %s: starting creating %d thread(s)\n",cmd_name,thread_count);
  for (int i=0; i<thread_count; i++) {
    if ((chunks[i].threaded=pthread_create(threads+i,NULL,task,chunks+i))!=0) {
      fprintf(stderr,"[LOG] %s: creating thread %d failed, fallback...\n",
              cmd_name,i);
    }
  }
  // fallback, main thread will do the job
  for (int i=0; i<thread_count; i++) {
    if (chunks[i].threaded!=0) {
      fprintf(stderr,"[LOG] %s: falling back %d\n",cmd_name,i);
      task(chunks+i);
    }
  }
  
  MYDEBUG("waiting %d results\n",thread_count);
  
  // Init result
  root.len = 0;
  root.letters = NULL;
  total_count = 0;

  //REDUCE: Accumulate results
  for (int i=0; i<thread_count; i++) {
    if (chunks[i].threaded==0) pthread_join(threads[i],(void **)&status);
    else status = chunks+i; // in case of fallback
    total_count += status->word_count;
    MYDEBUG("a thread found %d words (%ld %ld)\n",status->word_count,status->start,status->len);
    merge_trees(&root,&(status->root));
    deallocate_tree(&(status->root));
  }
  fprintf(stderr,"[LOG] %s: Found %d words\n",cmd_name,total_count);

#ifdef TIMED
  gettimeofday(&time_end,NULL);
  if (time_start.tv_usec>time_end.tv_usec) {
    time_end.tv_usec += 1000000;
    time_end.tv_sec--;
  }
  fprintf(stderr,"[LOG] %s: ELAPSED %4ld,%06d\n",
          cmd_name,
          time_end.tv_sec-time_start.tv_sec,
          time_end.tv_usec-time_start.tv_usec);
#endif // TIMED

  // Show results
  print_tree(&root);
  deallocate_tree(&root);
  
  exit(EXIT_SUCCESS);
}
