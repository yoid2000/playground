#include "./filters.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// externs needed to keep compiler from warning
extern bucket *dupBucket(bucket *arg1);
extern float getNormal(float arg1, float arg2);
extern float getFixedNormal(uint64_t arg1, float arg2, float arg3);

bucket **storedFilters;
int sfIndex; 	// index into above list
int maxSfIndex;

initDefense(int maxBuckets) {
  storedFilters = (bucket **) calloc(maxBuckets, sizeof(bucket *));
  sfIndex = 0;
  maxSfIndex = maxBuckets;
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
// returns a noisy integer count (not rounded)
int
putBucketDefend(bucket *bp) 
{
  int i, j, overlap;
  compare c;
  bucket *fbp;	// final bucket
  bucket *bp1, *bp2;
  int noisyCount;

  makeFilterFromBucket(bp);
  if (sfIndex < maxSfIndex) {
    j = sfIndex;
    storedFilters[sfIndex++] = bp;
  }
  else {
    printf("Run out of room for filters!\n");
    exit(1);
  }

  fbp = dupBucket(bp);
  for (i = 0; i < j; i++) {
    bp1 = storedFilters[i];
    bp2 = storedFilters[j];
    compareFullFilters(bp1, bp2, &c);
    overlap = (int) ((float) c.overlap * (float) 1.5625);
    if (i == (j-1)) {
      //printf("               %d, %d, %d, %d (%d, %d) (%d, %d)\n", i, j, c.overlap, overlap, c.first, c.common, bp1->bsize, bp2->bsize);
    }
    if (overlap > NEAR_MATCH_THRESHOLD) {
      if (sizesAreClose(bp1, bp2)) {
      // Find non-overlapping users (sort and walk):
        sortBucketList(bp1);
        sortBucketList(bp1);
      // Count non-overlapping users in high-touch hash
      // Remove high-touch users from near-match buckets
      }
    }
  }

  noisyCount = computeNoisyCount(fbp);
  freeBucket(fbp);
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
