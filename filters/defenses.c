#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <search.h>
#include "./filters.h"
#include "./hightouch.h"

// externs needed to keep compiler from warning
extern bucket *makeBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);
extern float getNormal(float arg1, float arg2);
extern float getFixedNormal(unsigned int arg1, float arg2, float arg3);
extern bucket *getNonOverlap(bucket *arg1, bucket *arg2,
                             bucket **arg3, 
                             bucket **arg4, 
                             bucket **arg5, 
                             bucket **arg6);


bucket **storedFilters;
int sfIndex; 	// index into above list
int maxSfIndex;

#define MAX_TOUCH_COUNT 20
int numTouches[MAX_TOUCH_COUNT];
int numComparisons;
int numNearMatch;
int numCloseSize;
int numSmallOverlap;
int numPutBuckets;
int numAllSuppressed;
int numAttackSuppressed;

initDefense(int maxBuckets) {
  storedFilters = (bucket **) calloc(maxBuckets, sizeof(bucket *));
  sfIndex = 0;
  maxSfIndex = maxBuckets;
  // this hash table much bigger than needed
  initHighTouchTable();
}

initDefenseStats()
{
  int i;

  for (i = 0; i < MAX_TOUCH_COUNT; i++) {
    numTouches[i] = 0;
  }
  numPutBuckets = 0;
  numComparisons = 0;
  numNearMatch = 0;
  numCloseSize = 0;
  numSmallOverlap = 0;
  numAttackSuppressed = 0;
  numAllSuppressed = 0;
}

computeDefenseStats(int numRounds)
{
  int i;

  printf("Num Touches Histogram:\n");
  for (i = 0; i < MAX_TOUCH_COUNT; i++) {
    if (numTouches[i] != 0) {
      printf("    %d: %.2f\n", i, (float)((float)numTouches[i]/(float)numRounds));
    }
  }
  printf("%.2f buckets, %.2f bucket pairs\n", (float)((float)numPutBuckets/(float)numRounds), (float)((float)numComparisons/(float)numRounds));
  printf("%.2f (%d%%) near matches\n", (float)((float)numNearMatch/(float)numRounds), 
          (int)((float)(numNearMatch*100)/(float) numComparisons));
  printf("%.2f (%d%%) close sizes\n", (float)((float)numCloseSize/(float)numRounds), 
          (int)((float)(numCloseSize*100)/(float) numNearMatch));
  printf("%.2f (%d%%) small overlaps\n", (float)((float)numSmallOverlap/(float)numRounds), 
          (int)((float)(numSmallOverlap*100)/(float) numCloseSize));
  printf("Average %.2f attack-suppressed users per checked bucket\n",
          (float)((float)numAttackSuppressed/(float) numSmallOverlap));
  printf("Average %.2f all-suppressed users per checked bucket\n",
          (float)((float)numAllSuppressed/(float) numSmallOverlap));
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
  countNumTouches(numTouches, MAX_TOUCH_COUNT);
  freeStoredFilters();
  free(storedFilters);
}

int
computeNoisyCount(bucket *bp)
{
  int i;
  float noise;
  unsigned int fix=0;

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

/*
 * bp is the new bucket, bp1 is the former bucket being compared against
 * returns NULL if no new bucket was created.
 *
 * calling routine must free returned bucket, if any
 */
bucket *
checkSizesAreClose(bucket *bp, bucket *bp1)
{
  bucket *nobp=NULL, *obp=NULL, *nobp1=NULL, *obp1=NULL;

  if (sizesAreClose(bp, bp1)) {
    numCloseSize++;
    // Find non-overlapping users (sort and walk):
    sortBucketList(bp);
    sortBucketList(bp1);
    getNonOverlap(bp, bp1, &nobp, &nobp1, &obp, &obp1);
    // Count non-overlapping users in high-touch hash
    if (((nobp->bsize + nobp1->bsize) < SIZE_CLOSE_THRESH) &&
        ((nobp->bsize + nobp1->bsize) > 0)) {
      numSmallOverlap++;
      // the combined non-overlap is small enough that
      // we consider the two buckets to be near matches.

      // touch the overlapping users from both buckets (note that
      // this means that, for the former bucket, I might be touching 
      // users more than once for the same bucket).
      touchNonOverlappingUsers(nobp);
      touchNonOverlappingUsers(nobp1);

      // Add non-suppressed non-overlap users to overlap bucket (thus
      // suppressing users subject to the ATTACK threshold)
      addNonSuppressedUsers(obp, nobp, HT_ATTACK);
      numAttackSuppressed += bp->bsize - obp->bsize;
      free(obp1);
      free(nobp);
      free(nobp1);
    }
  }
  // not enough overlap to generate a new overlap bucket
  return(obp);
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
  bucket *allbp, *attackbp;
  int noisyCount;

  numPutBuckets++;
  makeFilterFromBucket(bp);
  if (sfIndex < maxSfIndex) {
    j = sfIndex;
    storedFilters[sfIndex++] = bp;
  }
  else {
    printf("Run out of room for filters!\n");
    exit(1);
  }

  attackbp = bp;
  for (i = 0; i < j; i++) {
    numComparisons++;
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
      numNearMatch++;
      // establish a parent-child link
      // for every new MtO and OtO combination, create a "composite"
      //     bucket (composed of combinations of children), and check for
      //     sizes are close etc.
      if ((attackbp = checkSizesAreClose(bp, bp1)) == NULL) {
        attackbp = bp;
      }
    }
  }
  // At this point, all users that should be suppressed because of the
  // ATTACK threshold have been removed.  But not users that should be
  // suppressed because of the higher ALL threshold.  So remove those
  // users now.
  allbp = makeBucket(attackbp->bsize);
  allbp->bsize = 0;

  // allbp has allocated space for all of attackbp, but no users:
  // copy non suppressed users into allbp
  addNonSuppressedUsers(allbp, attackbp, HT_ALL);
  numAllSuppressed += attackbp->bsize - allbp->bsize;
  // note bp never freed, because stored for later comparisons

  noisyCount = computeNoisyCount(allbp);

  if (attackbp != bp) { 
    // bucket was created in checkSizesAreClose()
    freeBucket(attackbp);
  }
  freeBucket(allbp);
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
