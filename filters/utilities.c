#include "./utilities.h"
#include <stdlib.h>
#include <stdio.h>

/* 
 * Allocates memory for a bucket of size bsize.
 * All entries in the bucket list are zero.
 */
bucket *
makeBucket(int bsize)
{
  int i;
  bucket *bp;

  bp = (bucket*) malloc(sizeof(bucket));
  bp->list = (uint64_t*) calloc(bsize, sizeof(uint64_t));
  bp->bsize = bsize;
  return(bp);
}

uint64_t
myRand64()
{
  return((lrand48()<<48) ^ (lrand48()<<16) ^ lrand48());
}

/* Ignore duplicate random numbers (on rare occasion, may be duplicate
 * user in a given bucket) */
bucket *
makeRandomBucket(int bsize)
{
  int i;
  uint64_t *lp;
  bucket *bp;

  bp = makeBucket(bsize);
  lp = bp->list;

  for (i = 0; i < bsize; i++) {
    *lp = myRand64();
    lp++;
  }
  return(bp);
}

freeBucket(bucket *bp)
{
  free((void *) bp->list);
  free((void *) bp);
}

printBucket(bucket* bp)
{
  int i;
  uint64_t *lp;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    printf("%4d: %lx\n", i, *lp);
    lp++;
  }
}



/*********** TESTS **********/

test_makeRandomBucket()
{
  bucket *bp;

  bp = makeRandomBucket(20);
  printBucket(bp);
  freeBucket(bp);
}
