#include "./filters.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <search.h>

// externs needed to keep compiler from warning
extern bucket *dupBucket(bucket *arg1);
extern float getNormal(float arg1, float arg2);
extern float getFixedNormal(uint64_t arg1, float arg2, float arg3);
extern bucket *getNonOverlap(bucket *arg1, bucket *arg2,
                             bucket **arg3, 
                             bucket **arg4, 
                             bucket **arg5, 
                             bucket **arg6);


bucket **storedFilters;
int sfIndex; 	// index into above list
int maxSfIndex;

initDefense(int maxBuckets) {
  storedFilters = (bucket **) calloc(maxBuckets, sizeof(bucket *));
  sfIndex = 0;
  maxSfIndex = maxBuckets;
  // this hash table much bigger than needed
  initHighTouchTable();
}

freeStoredFilters()
{
  int i;

  for (i = 0; i < sfIndex; i++) {
    freeBucket(storedFilters[i]);
  }
  sfIndex = 0;
}

endDefense() {
  freeStoredFilters();
  free(storedFilters);
}

int
computeNoisyCount(bucket *bp)
{
  int i;
  float noise;
  uint64_t fix=0;

  for (i = 0; i < bp->bsize; i++) {
    fix ^= bp->list[i];
  }
  noise = getNormal(0.0, 2.0);	// random noise, SD=2
  noise += getFixedNormal(fix, 0.0, 1.5);  // fixed noise, SD=1.5
  // note: no rounding to 5 here.  I figure that it doesn't defeat any
  // attack, and makes it harder to understand what attacks work...
  noise = roundf(noise);
  return((int)(bp->bsize + noise));
}

// the following threshold should catch most though not quite all near
// matching buckets.  Because of added noise, it is not critical
// that we catch all near matches.
#define NEAR_MATCH_THRESHOLD 90
/*
 *  This routine operates on one bucket at a time.  This means that if
 *  the current bucket is a near match with a former bucket, then the
 *  non-overlapping (victim) nodes in the former bucket will not have
 *  been suppressed.  This is a worst-case scenario.  A better scenario
 *  would be where all attack buckets are part of the same query, and
 *  so victims in all attack buckets can be suppressed.  (Of course, a
 *  smart attacker wouldn't do it this way  ;)
 *
 * returns a noisy integer count (not rounded to nearest 5)
 */
int
putBucketDefend(bucket *bp) 
{
  int i, j, overlap;
  compare c;
  bucket *fbp;	// final bucket
  bucket *bp1;
  bucket *nobp, *obp, *nobp1, *obp1;
  bucket *noisebp;
  int noisyCount;

  nobp=NULL; obp=NULL; nobp1=NULL; obp1=NULL;
  makeFilterFromBucket(bp);
  if (sfIndex < maxSfIndex) {
    j = sfIndex;
    storedFilters[sfIndex++] = bp;
  }
  else {
    printf("Run out of room for filters!\n");
    exit(1);
  }

  noisebp = bp;
  // zzzz we need to remove high-touch users here....later
  for (i = 0; i < j; i++) {
    bp1 = storedFilters[i];
    compareFullFilters(bp1, bp, &c);
    overlap = (int) ((float) c.overlap * (float) 1.5625);
/*
    if (i == (j-1)) {
      printf("               %d, %d, %d, %d (%d, %d) (%d, %d)\n", 
             i, j, c.overlap, overlap, c.first, c.common, 
             bp1->bsize, bp->bsize);
    }
*/
    if (overlap > NEAR_MATCH_THRESHOLD) {
      if (sizesAreClose(bp, bp1)) {
        // Find non-overlapping users (sort and walk):
        sortBucketList(bp);
        sortBucketList(bp1);
        getNonOverlap(bp, bp1, &nobp, &nobp1, &obp, &obp1);
        // Count non-overlapping users in high-touch hash
        if (((nobp->bsize + nobp1->bsize) < SIZE_CLOSE_THRESH) &&
            ((nobp->bsize + nobp1->bsize) > 0)) {
printf("%d, %d (%d, %d)\n", nobp->bsize, nobp1->bsize, bp->bsize, bp1->bsize);
          // the combined non-overlap is small enough that
          // we consider the two buckets to be near matches.

          // touch the overlapping users from both buckets (note that
          // this means that, for the former bucket, I might be touching 
          // users more than once for the same bucket).
printf("bp:\n"); printBucket(bp);
printf("bp1:\n"); printBucket(bp1);
printf("nobp:\n"); printBucket(nobp);
printf("nobp1:\n"); printBucket(nobp1);
printf("obp:\n"); printBucket(obp);
printf("obp1:\n"); printBucket(obp1);
printf("1\n");
          touchNonOverlappingUsers(nobp);
printf("2\n");
          touchNonOverlappingUsers(nobp1);

          // Add non-suppressed non-overlap users to overlap bucket
          addNonSuppressedUsers(obp, nobp, HT_ATTACK);
          noisebp = obp;
        }
      }
    }
  }

  noisyCount = computeNoisyCount(noisebp);
  if (nobp) { freeBucket(nobp); }
  if (obp) { freeBucket(obp); }
  if (nobp1) { freeBucket(nobp1); }
  if (obp1) { freeBucket(obp1); }
  return(noisyCount);
}

int
putBucketCrap(bucket *bp) 
{
  int noisyCount;

  noisyCount = computeNoisyCount(bp);
  freeBucket(bp);
  return(noisyCount);
}

int
putBucket(bucket *bp, int defend)
{
  if (defend) {return(putBucketDefend(bp));}
  else {return(putBucketCrap(bp));}
}
