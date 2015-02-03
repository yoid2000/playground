#include <stdio.h>
#include <time.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>
#include "./filters.h"

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeBucket(int arg1);
extern int exceedsNoisyThreshold(float arg1, float arg2, float arg3);

/* 
 * Allocates memory for a bucket of size bsize.
 * All entries in the bucket list and filters are zero.
 */
bucket *
makeBucket(int bsize)
{
  int i, j;
  bucket *bp;

  bp = (bucket*) malloc(sizeof(bucket));
  bp->sorted = 0;
  bp->list = (unsigned int *) calloc(bsize, sizeof(unsigned int));
  bp->bsize = bsize;
  initFilter(bp);

  return(bp);
}

bucket *
dupBucket(bucket *from)
{
  int i;
  bucket *bp;
  unsigned int *lfrom, *lp;

  bp = makeBucket(from->bsize);
  bp->sorted = from->sorted;
  lp = bp->list;
  lfrom = from->list;

  for (i = 0; i < from->bsize; i++) {
    *lp = *lfrom;
    lp++;  lfrom++;
  }
  return(bp);
}


bucket *
combineBuckets(bucket *bp1, bucket *bp2)
{
  int i;
  bucket *bp;
  unsigned int *lfrom, *lp;

  bp = makeBucket(bp1->bsize + bp2->bsize);
  bp->sorted = 0;
  lp = bp->list;

  lfrom = bp1->list;
  for (i = 0; i < bp1->bsize; i++) {
    *lp = *lfrom;
    lp++;  lfrom++;
  }

  lfrom = bp2->list;
  for (i = 0; i < bp2->bsize; i++) {
    *lp = *lfrom;
    lp++;  lfrom++;
  }
  return(bp);
}

/* 
 * causes the two buckets to have numMatches identical entries
 */
makeCompareBucketFixed(bucket* bp1, bucket* bp2, int numMatches) {
  int i;
  unsigned int *lp1, *lp2;

  if (numMatches > bp1->bsize) {numMatches = bp1->bsize;}
  if (numMatches > bp2->bsize) {numMatches = bp2->bsize;}
  
  lp1 = bp1->list;
  lp2 = bp2->list;
  for (i = 0; i < numMatches; i++) {
    *lp2 = *lp1;
    lp1++;
    lp2++;
  }
}

/*
 * I think this compare function is not exactly "right", in the sense
 * that it is not producing monotonically increasing values in the
 * list (maybe because of signed versus unsigned????).  However, this
 * is not a problem, since we only care that two buckets are
 * sorted the same way, and that getNonOverlap uses the same compare
 * function, and not that they are individually monotonically
 * increasing.
 */
int userCmpFunc (const void * a, const void * b)
{
  return ( (*(unsigned int *)a) - (*(unsigned int *)b) );
}

sortBucketList(bucket *bp)
{
  if (bp->sorted == 0) {
    bp->sorted = 1;
    qsort(bp->list, bp->bsize, sizeof(unsigned int), userCmpFunc);
  }
}

/*
 *  Finds non-overlapping users between the two buckets, 
 *  and makes four new buckets:  two with only the non-overlapping
 *  users from each of the buckets respectively, and two
 *  containing only the overlapping users of the two buckets.
 *  The returned buckets have the same amount of allocated space in the
 *  user lists as the corresponding submitted buckets.  This is
 *  important because more users may be added to the overlap buckets
 *  by the calling routine.  Note that the two overlapping buckets are
 *  identical (in membership, though not in allocated memory).  This is
 *  maybe a bit of unecessary extra work, but it makes things cleaner 
 *  on the calling side.
 *  The calling routine must free the four new buckets.
 */
bucket *
getNonOverlap(bucket *bp1, 	// submitted bucket 1 (sorted)
              bucket *bp2,      // submitted bucket 2 (sorted)
              bucket **r_nobp1,   // non-overlap of bp1
              bucket **r_nobp2,   // non-overlap of bp2
              bucket **r_obp1,    // overlap of bp1
              bucket **r_obp2)    // overlap of bp2
{
  unsigned int *l1, *l2, *l1_last, *l2_last;
  bucket *nobp1, *nobp2, *obp1, *obp2;	// return buckets
  unsigned int *nol1, *nol2, *ol1, *ol2;
  int compare;

  // We pessimistically make the worst-case sized buckets.
  nobp1 = makeBucket(bp1->bsize);
  obp1 = makeBucket(bp1->bsize);
  nobp2 = makeBucket(bp2->bsize);
  obp2 = makeBucket(bp2->bsize);
  // set up all my list pointers
  l1 = bp1->list;
  l1_last = &(bp1->list[bp1->bsize]);
  l2 = bp2->list;
  l2_last = &(bp2->list[bp2->bsize]);
  nol1 = nobp1->list;
  ol1 = obp1->list;
  nol2 = nobp2->list;
  ol2 = obp2->list;
  // and initialize the overlap and non-overlap bucket sizes
  nobp1->bsize = 0;
  obp1->bsize = 0;
  nobp2->bsize = 0;
  obp2->bsize = 0;
  while(1) {
    if ((l2 == l2_last) && (l1 == l1_last)) {
      break;
    }
    else if ((l2 == l2_last) && (l1 < l1_last)) {
      *nol1 = *l1;
      nol1++;
      l1++;
      nobp1->bsize++;
    }
    else if ((l2 < l2_last) && (l1 == l1_last)) {
      *nol2 = *l2;
      nol2++;
      l2++;
      nobp2->bsize++;
    }
    else {
      compare = userCmpFunc((const void *)l2, (const void *)l1);
      if (compare == 0) {
        if (l2 < l2_last) {
          *ol2 = *l2;
          ol2++;
          l2++;
          obp2->bsize++;
        }
        if (l1 < l1_last) {
          *ol1 = *l1;
          ol1++;
          l1++;
          obp1->bsize++;
        }
      }
      else if (compare < 0) {
        if (l2 < l2_last) {
          *nol2 = *l2;
          nol2++;
          l2++;
          nobp2->bsize++;
        }
      }
      else if (compare > 0) {
        if (l1 < l1_last) { 
          *nol1 = *l1;
          nol1++;
          l1++;
          nobp1->bsize++;
        }
      }
    }
  }
  // set the return pointers
  *r_nobp1 = nobp1;
  *r_nobp2 = nobp2;
  *r_obp1 = obp1;
  *r_obp2 = obp2;
}

/* Ignore duplicate random index (on occasion, may be duplicate
 * user in a given bucket) */
bucket *
makeRandomBucketFromList(int bsize, bucket *userList)
{
  int i, index;
  unsigned int *lp;
  bucket *bp;

  bp = makeBucket(bsize);
  lp = bp->list;

  for (i = 0; i < bsize; i++) {
    index = getRandInteger(0, userList->bsize);
    *lp = userList->list[index];
    lp++;
  }
  return(bp);
}

/* Ignore duplicate random numbers (on rare occasion, may be duplicate
 * user in a given bucket) */
bucket *
makeRandomBucket(int bsize)
{
  int i;
  unsigned int *lp;
  bucket *bp;

  bp = makeBucket(bsize);
  lp = bp->list;

  for (i = 0; i < bsize; i++) {
    *lp = lrand48();
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
  unsigned int *lp;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    printf("%4d: %x\n", i, *lp);
    lp++;
  }
}

/*********** TESTS **********/


do_getNonOverlap(int bsize1, int bsize2, int overlap)
{
  bucket *bp1, *bp2;
  bucket *nobp1, *nobp2, *obp1, *obp2;
  int i, j, foundDups1, foundDups2;

  nobp1 = NULL; nobp2 = NULL; obp1 = NULL; obp2 = NULL;
  bp1 = makeRandomBucket(bsize1);
  bp2 = makeRandomBucket(bsize2);
  makeCompareBucketFixed(bp1, bp2, overlap);
  getNonOverlap(bp1, bp2, &nobp1, &nobp2, &obp1, &obp2);
  if ((nobp1 == NULL) || (nobp2 == NULL) || (obp1 == NULL) || (obp2 == NULL)) {
    printf("test_getNonOverlap FAILED!!!  NULL return pointer\n");
    goto finish;
  }
  if (bp1->bsize != (nobp1->bsize + obp1->bsize)) {
    printf("test_getNonOverlap FAILED!!! (bp1: got %d = (%d+%d), expected %d)\n",
             (nobp1->bsize + obp1->bsize), bp1->bsize,
             nobp1->bsize, obp1->bsize);
    goto finish;
  }
  if (bp2->bsize != (nobp2->bsize + obp2->bsize)) {
    printf("test_getNonOverlap FAILED!!! (bp2: got %d = (%d+%d), expected %d)\n",
             (nobp2->bsize + obp2->bsize), bp2->bsize,
             nobp2->bsize, obp2->bsize);
    goto finish;
  }
  if (overlap != obp1->bsize) {
    printf("test_getNonOverlap FAILED!!! (overlap1: got %d, expected %d)\n",
           overlap, obp1->bsize);
    goto finish;
  }
  if (overlap != obp2->bsize) {
    printf("test_getNonOverlap FAILED!!! (overlap2: got %d, expected %d)\n",
           overlap, obp2->bsize);
    goto finish;
  }

  for (i = 0; i < nobp1->bsize; i++) {
    foundDups1 = 0;
    foundDups2 = 0;
    for (j = 0; j < bp1->bsize; j++) {
      if (nobp1->list[i] == bp1->list[j]) {
        foundDups1++;
      }
    }
    for (j = 0; j < bp2->bsize; j++) {
      if (nobp1->list[i] == bp2->list[j]) {
        foundDups2++;
      }
    }
    if (foundDups1 != 1) {
      printf("test_getNonOverlap FAILED!!! dup1, found %d dups, expected 1\n",
              foundDups1);
      goto finish;
    }
    if (foundDups2 != 0) {
      printf("test_getNonOverlap FAILED!!! dup1, found %d dups, expected 0\n",
              foundDups2);
      goto finish;
    }
  }

  for (i = 0; i < nobp2->bsize; i++) {
    foundDups1 = 0;
    foundDups2 = 0;
    for (j = 0; j < bp1->bsize; j++) {
      if (nobp2->list[i] == bp1->list[j]) {
        foundDups2++;
      }
    }
    for (j = 0; j < bp2->bsize; j++) {
      if (nobp2->list[i] == bp2->list[j]) {
        foundDups1++;
      }
    }
    if (foundDups1 != 1) {
      printf("test_getNonOverlap FAILED!!! dup2, found %d dups, expected 1\n",
              foundDups1);
      goto finish;
    }
    if (foundDups2 != 0) {
      printf("test_getNonOverlap FAILED!!! dup2, found %d dups, expected 0\n",
              foundDups2);
      goto finish;
    }
  }

  freeBucket(bp1);
  freeBucket(bp2);
  freeBucket(nobp1);
  freeBucket(nobp2);
  freeBucket(obp1);
  freeBucket(obp2);
  return;
finish:
  printf("bsize1 = %d, bsize2 = %d, overlap = %d\n", bsize1, bsize2, overlap);
  printf("bp1:\n");
  printBucket(bp1);
  printf("bp2:\n");
  printBucket(bp2);
  printf("nobp1:\n");
  printBucket(nobp1);
  printf("nobp2:\n");
  printBucket(nobp2);
  printf("obp1:\n");
  printBucket(obp1);
  printf("obp2:\n");
  printBucket(obp2);
  exit(1);
}

test_getNonOverlap() 
{
  int i;
  int bsize1, bsize2, overlap, min;

  do_getNonOverlap(30, 30, 20);
  do_getNonOverlap(30, 30, 30);
  do_getNonOverlap(2, 2, 1);
  do_getNonOverlap(30, 40, 30);
  do_getNonOverlap(30, 40, 0);
  for (i = 0; i < 1000; i++) {
    bsize1 = getRandInteger(1, 200);
    bsize2 = getRandInteger(1, 200);
    min = (bsize1 < bsize2)?bsize1:bsize2;
    overlap = getRandInteger(0, min);
    do_getNonOverlap(bsize1, bsize2, overlap);
  }
  printf("test_getNonOverlap PASSED\n");
}

test_combineBuckets() 
{
  bucket *bp1, *bp2, *new;
  unsigned int *l1, *l2, *lnew;
  int i;

  bp1 = makeRandomBucket(439);
  bp2 = makeRandomBucket(12);
  new = combineBuckets(bp1, bp2);

  l1 = bp1->list;
  l2 = bp2->list;
  lnew = new->list;

  for (i = 0; i < bp1->bsize; i++) {
    if (*l1 != *lnew) {
      printf("test_combineBuckets FAILED!!! \n");
      printBucket(bp1);
      printBucket(bp2);
      printBucket(new);
    }
    l1++;  lnew++;
  }
  for (i = 0; i < bp2->bsize; i++) {
    if (*l2 != *lnew) {
      printf("test_combineBuckets FAILED!!! \n");
      printBucket(bp1);
      printBucket(bp2);
      printBucket(new);
    }
    l2++;  lnew++;
  }

  printf("test_combineBuckets() passed\n");
  freeBucket(bp1);
  freeBucket(bp2);
  freeBucket(new);
}

test_makeRandomBucket()
{
  bucket *bp;

  bp = makeRandomBucket(20);
  printf("\nThe following IDs should all be 32 bits long\n");
  printBucket(bp);
  freeBucket(bp);
  bp = makeRandomBucket(100000);
  printf("\nThe following counts of the high and low bits should be\nroughly half the total number of entries\n");
  countHighAndLowBits(bp);
}
