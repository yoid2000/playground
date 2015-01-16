#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "./utilities.h"
#include "./filters.h"

int
getRandInteger(int min_size, int max_size)
{
  int ret;

  float frac = (float) (max_size+1-min_size) / (float) 0x7fffffff;

  ret = (min_size + (int) ((float) lrand48() * frac));
  if (ret < min_size) {
    printf("ERROR: bad getRandInteger, %d less than min %d\n", ret, min_size);
    ret = min_size;
  }
  if (ret > max_size) {
    printf("ERROR: bad getRandInteger, %d greater than max %d\n", ret, max_size);
    ret = max_size;
  }
  return(ret);
}

/*
 * This routine returns a random number between -0.25 and 0.25.
 * It is used for scatter plots to prevent identical points from
 * obscuring each other.
 */
float
myskew()
{
  float ran;

  ran = (float)drand48();
  ran /= (float) 2.0;
  ran -= (float) 0.25;
  return(ran);
}

printStats(unsigned char *str, mystats *s, int header)
{
  if (header) {
    printf("\n| Total | Av | SD | Min | Max | Desc |\n");
    printf("| --- | --- | --- | --- | --- | --- |\n");
  }
  printf("| %d | %.2f | %.2f | %.2f | %.2f | %s |\n",
      s->total, s->av, s->sd, s->min, s->max, str);
}

getStats(mystats *s, float *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = 100000000000.0;
  s->max = 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow((x[i] - s->av), 2);
    s->min = (x[i] < s->min)?x[i]:s->min;
    s->max = (x[i] > s->max)?x[i]:s->max;
  }
  s->var = sum1 / (float)n;
  s->sd = sqrt(s->var);
}

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
  bp->list = (uint64_t*) calloc(bsize, sizeof(uint64_t));
  bp->bsize = bsize;
  for (i = 0; i < FILTERS_PER_BUCKET; i++) {
    bp->filters[i].level =0;
    for (j = 0; j < FILTER_LEN; bp->filters[i].filter[j++] = 0);
  }

  return(bp);
}

bucket *
dupBucket(bucket *from)
{
  int i;
  bucket *bp;
  uint64_t *lfrom, *lp;

  bp = makeBucket(from->bsize);
  lp = bp->list;
  lfrom = from->list;

  for (i = 0; i < from->bsize; i++) {
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
  uint64_t *lp1, *lp2;

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

countHighAndLowBits(bucket *bp)
{
  int i;
  int high = 0;
  int low = 0;
  uint64_t *lp;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    if (*lp & 0x8000000000000000) {high++;}
    if (*lp & 0x01) {low++;}
    lp++;
  }
  printf("%d high and %d low of %d entries\n\n", high, low, bp->bsize);
}


/*********** TESTS **********/

test_makeRandomBucket()
{
  bucket *bp;

  bp = makeRandomBucket(20);
  printf("\nThe following IDs should all be 64 bits long\n");
  printBucket(bp);
  freeBucket(bp);
  bp = makeRandomBucket(100000);
  printf("\nThe following counts of the high and low bits should be\nroughly half the total number of entries\n");
  countHighAndLowBits(bp);
}
