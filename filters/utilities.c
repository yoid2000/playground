#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "./utilities.h"
#include "./filters.h"
#include "./normalDist.h"

int
getRandInteger(int min_size, int max_size)
{
  int ret;

  float frac = (float) (max_size+1-min_size) / (float) 0x7fffffff;

  ret = (min_size + (int) ((float) lrand48() * frac));
  if (ret < min_size) {
    //printf("ERROR: bad getRandInteger, %d less than min %d\n", ret, min_size);
    ret = min_size;
  }
  if (ret > max_size) {
    //printf("ERROR: bad getRandInteger, %d greater than max %d\n", ret, max_size);
    ret = max_size;
  }
  return(ret);
}

float
getRandFloat(float min, float max)
{
  float ran;
  ran = (float)drand48();
  ran *= (max - min);
  ran += min;
  return(ran);
}

float
getNormal(float mean, float sd) {
  int index;
  float ran;

  index = getRandInteger(0, 9999);
  ran = normalDist[index];
  ran *= sd;
  ran += mean;
  return(ran);
}

/*
 *  Returns 1 if threshold exceeded, 0 otherwise
 *
 *  Threshold is considered exceeded if the value + noise
 *  is greater than the hardThresh, where noise is generated
 *  according to the standard deviation sd.
 */
int
exceedsNoisyThreshold(float hardThresh, float value, float sd)
{
  float noise;

  noise = getNormal(0.0, sd);
//printf("%.2f\n", noise);
  if ((value + noise) > hardThresh) { return(1); }
  else { return(0); }
}


float
getFixedNormal(uint64_t fix, float mean, float sd) {
  float ran;

  fix &= 0x1fff;
  ran = normalDist[fix];
  ran *= sd;
  ran += mean;
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

getStatsFloat(mystats *s, float *x, int n)
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

getStatsChar(mystats *s, unsigned char *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = (float) 100000000000.0;
  s->max = (float) 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + (float) x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow(((float)x[i] - s->av), 2);
    s->min = ((float)x[i] < s->min)?(float)x[i]:s->min;
    s->max = ((float)x[i] > s->max)?(float)x[i]:s->max;
  }
  s->var = sum1 / (float)n;
  s->sd = sqrt(s->var);
}

// This is the exact same as getStatsChar, except for the
// incoming array (must be a cleaner way to code this....)
getStatsInt(mystats *s, int *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = (float) 100000000000.0;
  s->max = (float) 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + (float) x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow(((float)x[i] - s->av), 2);
    s->min = ((float)x[i] < s->min)?(float)x[i]:s->min;
    s->max = ((float)x[i] > s->max)?(float)x[i]:s->max;
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
  bp->sorted = 0;
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
  uint64_t *lfrom, *lp;

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

// I think this compare function is not exactly "right", in the sense
// that it is not producing monotonically increasing values in the
// list (maybe because of signed versus unsigned????).  However, this
// shouldn't be a problem, since we only care that two buckets are
// sorted the same way, not that they are individually monotonically
// increasing.
int userCmpFunc (const void * a, const void * b)
{
  return ( (*(uint64_t *)a) - (*(uint64_t *)b) );
}

sortBucketList(bucket *bp)
{
  if (bp->sorted == 0) {
    bp->sorted = 1;
    qsort(bp->list, bp->bsize, sizeof(uint64_t), userCmpFunc);
  }
}

bucket *
getNonOverlap(bucket *bp1, bucket *bp2)
{
  uint64_t *l1, *l2, *nol, *l1_last, *l2_last;
  bucket *nobp;	// non-overlap bucket
  int compare;


  // We pessimistically make the worst-case sized bucket.  Since we
  // define non-overlap as coming from either bucket, the worst case
  // would be all users in both buckets.
  nobp = makeBucket(bp1->bsize + bp2->bsize);
  l1 = bp1->list;
  l1_last = &(bp1->list[bp1->bsize]);
  l2 = bp2->list;
  l2_last = &(bp2->list[bp2->bsize]);
  nol = nobp->list;
  nobp->bsize = 0;
  while(1) {
    if ((l2 == l2_last) && (l1 == l1_last)) {
      break;
    }
    else if ((l2 == l2_last) && (l1 < l1_last)) {
      *nol = *l1;
      nol++;
      l1++;
      nobp->bsize++;
    }
    else if ((l2 < l2_last) && (l1 == l1_last)) {
      *nol = *l2;
      nol++;
      l2++;
      nobp->bsize++;
    }
    else {
      compare = userCmpFunc((const void *)l2, (const void *)l1);
      if (compare == 0) {
        if (l2 < l2_last) { l2++;}
        if (l1 < l1_last) { l1++;}
      }
      else if (compare < 0) {
        if (l2 < l2_last) {
          *nol = *l2;
          nol++;
          l2++;
          nobp->bsize++;
        }
      }
      else if (compare > 0) {
        if (l1 < l1_last) { 
          *nol = *l1;
          nol++;
          l1++;
          nobp->bsize++;
        }
      }
    }
  }
  return(nobp);
}

uint64_t
myRand64()
{
  return((lrand48()<<48) ^ (lrand48()<<16) ^ lrand48());
}

/* Ignore duplicate random index (on occasion, may be duplicate
 * user in a given bucket) */
bucket *
makeRandomBucketFromList(int bsize, bucket *userList)
{
  int i, index;
  uint64_t *lp;
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

do_getNonOverlap(int bsize1, int bsize2, int overlap)
{
  bucket *bp1, *bp2, *nbp;
  int i, j, foundDups;

  bp1 = makeRandomBucket(bsize1);
  bp2 = makeRandomBucket(bsize2);
  makeCompareBucketFixed(bp1, bp2, overlap);
  nbp = getNonOverlap(bp1, bp2);
  if (nbp->bsize != bsize1 + bsize2 - (overlap * 2)) {
    printf("test_getNonOverlap FAILED!!! (got %d, expect %d), bsize1 = %d, bsize2 = %d, overlap = %d\n", nbp->bsize, bsize1 + bsize2 - (overlap * 2), bsize1, bsize2, overlap);
    goto finish;
  }
  for (i = 0; i < nbp->bsize; i++) {
    foundDups = 0;
    for (j = 0; j < bp1->bsize; j++) {
      if (nbp->list[i] == bp1->list[j]) {
        foundDups++;
      }
    }
    for (j = 0; j < bp2->bsize; j++) {
      if (nbp->list[i] == bp2->list[j]) {
        foundDups++;
      }
    }
    if (foundDups != 1) {
      printf("test_getNonOverlap FAILED!!! (found %d dups), bsize1 = %d, bsize2 = %d, overlap = %d\n", foundDups, bsize1, bsize2, overlap);
      goto finish;
    }
  }

  freeBucket(bp1);
  freeBucket(bp2);
  freeBucket(nbp);
  return;
finish:
  printf("bp1:\n");
  printBucket(bp1);
  printf("bp2:\n");
  printBucket(bp2);
  printf("nbp:\n");
  printBucket(nbp);
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
  uint64_t *l1, *l2, *lnew;
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

test_getNormal()
{
  float *samples;
  int i;
  mystats S;
  float mean=1.0, sd=5.0;

  samples = (float *) calloc(10000, sizeof(float));

  getStatsFloat(&S, normalDist, 10000);
  printf("normalDist[] has mean %.2f and sd %.2f\n",
           S.av, S.sd);

  for (i = 0; i < 10000; i++) {
    samples[i] = getNormal(mean, sd);
  }
  getStatsFloat(&S, samples, 10000);
  printf("getNormal for mean %.2f and sd %.2f: mean %.2f, sd %.2f\n",
           mean, sd, S.av, S.sd);
}

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
//printf("                    (%lx, %lx)\n", l1, l1_last);
//printf("                    (%lx, %lx)\n", l2, l2_last);
//printf("C=0\n");
//printf("     C<0\n");
//printf("     2<L (%lx, %lx)\n", (*l1 & 0xff00000000000000) >> 48, (*l2 & 0xff00000000000000) >> 48);
//printf("C>0\n");
//printf("1<L (%lx, %lx)\n", (*l1 & 0xff00000000000000) >> 48, (*l2 & 0xff00000000000000) >> 48);
