# MapReduce
A program to compute the number of word in a file.

A word is a sequence of letters a-zA-Z, and are all lowercase converted, i.e.
`BaN` and `ban` are the same word. Original spec didn't exactly mention what to do with non alphabetic and non separators chars, we decided to consider every non alphabetic as separator.

See [the original specification](MapReduce.pdf).

Author: Jean-Baptiste Yunès, Jean.Baptiste.Yunes@free.fr

Date: Nov. 2015

## How it works?

- MAP: the file is cut in N chunks (starting offset, length), each one is parsed by a dedicated thread. A thread computes a prefix tree of the words it finds. At each level of the tree and for each letter at this level a count of words ending with this letter is updated.

- REDUCE: the main thread synchronizes on thread termination and accumulate the results. A final prefix tree is built by accumulated partial results into it.

The prefix tree for words `a`, `a`, `an`, `at`, `ban`, `the` looks like:

           |
    lvl1   a,2----------b,0---t,0
           |            |     |
    lvl2   n,1---t,1    a,0   h,0
                        |     |
    lvl3                n,1   e,1
    
## Install
A `Makefile` is provided:

- `all` to generate executable file;
- `test` to build the executable file and make some tests
- `clean` to clean the project (executable and objects)
- `status` to obtain the git status (non commited files)

#### Problems solved/adressed

**Word boundary**.
To obtain the real count of words, one must ensure that no word is missed.
The problem is that one cannot simply cut the file into exactly equals chunks, as a naive cut may start a chunk in the moddle of a word.
Then, before mapping the main thread adjusts begin and ending points of chunks to appropriate positions: position of the beginning of next following word of the theoretical position.

**Open files**. As each thread needs its own open file, an upper limit may be enforced by the system, so the number of thread is reduced to the maximum opened file descriptor on the file.

**Thread creation**. Thread creation may fail, in that case, fallback to the main thread is used; all remaining chunks after the failure are handled by the main thread.

## Tests
Several tests have been made.

Some Test are included in the build procedure (make all, or make test) use *Lorem Ipsum* generated files with different number of threads.

Thread creation failure tested in various situations.

Tests have been made with different type of files: source code, binaries.
As it is difficult to verify the accuracy, these have not been included,
but results at least seems correct.

Tests have been made on:

- MacOSX 10.11.1 El Capitan, Apple LLVM version 7.0.0 (clang-700.1.76), POSIX Threads, X86_64, two cores with four threads each
- Linux 4.2.5 Fedora 22 TwentyTwo, gcc 5.1.1, POSIX Threads, X86_64

## Performances
Measuring thread computation is highly non portable and a primitive time measure is provided by timestamping just before MAP and just after REDUCE. On OSX El Capitan physical thread concurrency 8) on a 350MB file :
    
    Threads     |  1   |  2   |  3   |  4   |  5   |  6   |  7   |  8
    Time (sec.) | 26   | 16   | 11   |  8.5 |  8.2 |  8.3 |  8   |  8
    
With a significantly high number of threads, time decreases down to 7.5, but some algorithmic choices may introduce border effects.

## Optimizations
Code has been naively developped. There is no tricky part. Several improvements may be adressed:

- search in arrays of letters is actually linear, dichotomy may improve performance.
- words are inserted in trees after having being entirely parsed, incremental insertion certainly improve performance by removing the need for dynamic elementary (re)allocations.
- thread launching is actually made after the determination of all chunks characteristics, a more incremental thread launching may be used, but performance will probably not severely improve the overall performance.
- MAP is obvious, we may determine an ideal chunk size and cut the file in pieces independently of the number of threads, and then map the chunks to a pool of threads (M-N mapping). This may probably improve performance on very huge files if the choice of size is able to minimize cache defaults...
- mapping the file in memory may improve performance. Not clear as the file is basically read from left to right and only once.

## TODO

- allocation problems not catched
- reallocation strategies to be tuned
- file locking (usually only advisory), private mapping may solve the problem?
