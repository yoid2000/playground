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
extern int exceedsNoisyThreshold(float arg1, float arg2, float arg3);
extern int countUsers(bucket *arg1);
extern bucket * getFirstChildComb(child_comb *c, 
                                  bucket *pbp,
                                  bucket *cbp,
                                  bucket **sf,
                                  int sfMax);
extern bucket * getNextChildComb(child_comb *c, 
                                  bucket *pbp,
                                  bucket *cbp,
                                  bucket **sf,
                                  int sfMax);



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
int numLowCount;
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
  numLowCount = 0;
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
      printf("    %d: %.2f\n", i, 
             (float)((float)numTouches[i]/(float)numRounds));
    }
  }
  printf("%.2f buckets (%.2f low count), %.2f bucket pairs\n", 
         (float)((float)numPutBuckets/(float)numRounds), 
         (float)((float)numLowCount/(float)numRounds), 
         (float)((float)numComparisons/(float)numRounds));
  printf("%.2f (%d%%) near matches\n", 
         (float)((float)numNearMatch/(float)numRounds), 
          (int)((float)(numNearMatch*100)/(float) numComparisons));
  printf("%.2f (%d%%) close sizes\n", 
         (float)((float)numCloseSize/(float)numRounds), 
          (int)((float)(numCloseSize*100)/(float) numNearMatch));
  printf("%.2f (%d%%) small overlaps\n", 
         (float)((float)numSmallOverlap/(float)numRounds), 
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
 * Doesn't matter which bucket is bp1 or bp2.
 * Returns 1 if buckets are true near matches
 * returns 0 otherwise
 */
int
checkNearMatchAndTouchNonOverlap(bucket *bp1, bucket *bp2)
{
  bucket *nobp2=NULL, *obp2=NULL, *nobp1=NULL, *obp1=NULL;

  if (sizesAreClose(bp1, bp2)) {
    // the exact bucket size (stored with the filter) provides
    // further evidence that this may be an attack.  So now we
    // need to re-query the other bucket and check exact overlap.
    numCloseSize++;
    // Find non-overlapping users (sort and walk):
    sortBucketList(bp1);
    sortBucketList(bp2);
    getNonOverlap(bp1, bp2, &nobp1, &nobp2, &obp1, &obp2);
    // Count non-overlapping users in high-touch hash
    if (((nobp2->bsize + nobp1->bsize) < SIZE_CLOSE_THRESH) &&
        ((nobp2->bsize + nobp1->bsize) > 0)) {
      numSmallOverlap++;
      // the combined non-overlap is small enough that
      // we consider the two buckets to be near matches.

      // touch the overlapping users from both buckets (note that
      // this means that, for the former bucket, I might be touching 
      // users more than once for the same bucket).
      touchNonOverlappingUsers(nobp2);
      touchNonOverlappingUsers(nobp1);
      free(obp1);
      free(obp2);
      free(nobp1);
      free(nobp2);
      return(1);
    }
  }
  return(0);
}

/*
 * sbp is the new bucket that I'm suppressing from.
 * bp1 is what I'm comparing against.  
 *
 * In OtO case, bp1 is the former bucket.
 * In MtO case where sbp is the parent, then bp1 is the composite child.
 * In MtO case where sdp is the child, then bp1 is the former parent.
 *
 * I might have already done some suppression on sbp.
 * This routine replaces sbp with a potentially more-
 * suppressed bucket, and frees sbp.
 *
 * calling routine must free returned bucket
 */
bucket *
getNonOverlapAndSuppress(bucket *sbp, bucket *bp1)
{
  bucket *nobp=NULL, *obp=NULL, *nobp1=NULL, *obp1=NULL;

  // Find non-overlapping users (sort and walk):
  sortBucketList(sbp);
  sortBucketList(bp1);

  getNonOverlap(sbp, bp1, &nobp, &nobp1, &obp, &obp1);
  // nobp is the non-overlap for sbp, and obp is the overlap
  // (for both buckets, actually).  obp + nobp = sbp.

  // Add non-suppressed non-overlap users to overlap bucket (thus
  // suppressing users subject to the ATTACK threshold)
  if (nobp->bsize) {
    addNonSuppressedUsers(obp, nobp, HT_ATTACK);
    numAttackSuppressed += sbp->bsize - obp->bsize;
  }
  freeBucket(obp1);
  freeBucket(nobp);
  freeBucket(nobp1);
  freeBucket(sbp);

  // not enough overlap to generate a new overlap bucket
  return(obp);
}

addChildLink(bucket *parent, int child_index)
{
  if (parent->numChildren >= MAX_CHILDREN) {
    printf("addChildLink: Exceeded MAX_CHILDREN!!\n");
    exit(1);
  }
  parent->children[parent->numChildren] = child_index;
  parent->numChildren++;
}

/*
 * bpb = parent, sbp = suppress
 * returns suppressed bucket sbp.  If returned sbp different than
 * calling sbp, then calling sbp will have been freed. 
 */
bucket *
checkAllChildCombinations(bucket *pbp,     // parent
                          bucket *cbp,     // child, if new bucket is child
                          bucket *sbp)     // suppress bucket
{
  bucket *cobp;
  child_comb c;
  bucket *nobp;
  int nearMatch;

  if ((cobp = getFirstChildComb(&c, pbp, cbp, storedFilters, sfIndex)) 
                                                                == NULL) {
    // not enough children, so done
    return(sbp);
  }
  // In a real implementation, the sizes check would be made before
  // re-querying the buckets.  Here in this simulation, however,
  // getFirstChildComb() builds the composite bucket as well.
  nearMatch = checkNearMatchAndTouchNonOverlap(cobp, pbp);
  if (nearMatch == 1) {
    // This is verified as a potential attack tuple.  Suppress
    // non-overlap users.
    if (cbp) {
      // new bucket is child, so get non-overlap against parent
      nobp = pbp;
    }
    else {
      // new bucket is parent, so get non-overlap with child composite
      nobp = cobp;
    }
    sbp = getNonOverlapAndSuppress(sbp, nobp);
  }
  // we keep checking even though we already found one child combination
  // there might be another....
  while (1) {
    if ((cobp = getNextChildComb(&c, pbp, cbp, storedFilters, sfIndex)) 
                                                               == NULL) {
      break;
    }
    // admittedly ugly that I'm repeating this code.  I don't think I
    // built the getFirstChildComb / getNextChildComb interface very
    // smartly...
    nearMatch = checkNearMatchAndTouchNonOverlap(cobp, pbp);
    if (nearMatch == 1) {
      if (cbp) {
        // new bucket is child, so get non-overlap against parent
        nobp = pbp;
      }
      else {
        // new bucket is parent, so get non-overlap with child composite
        nobp = cobp;
      }
      sbp = getNonOverlapAndSuppress(sbp, nobp);
    }
  }
  return(sbp);
}

// the following threshold should catch most though not quite all near
// matching buckets.  Because of added noise, it is not critical
// that we catch all near matches.
#define NEAR_MATCH_THRESHOLD 90
#define LOW_COUNT_SOFT_THRESHOLD 5
#define LOW_COUNT_HARD_THRESHOLD 1
#define LOW_COUNT_SD 1
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
  bucket *allbp, *suppressbp, *temp;
  int noisyCount, nearMatch, childAdded;

  numPutBuckets++;

  countUsers(bp);

  // do low-count filter early-on.  If fails, then we don't even
  // bother to remember the bucket.   Attacker can't do anything with this.
  if ((bp->bsize <= LOW_COUNT_HARD_THRESHOLD) || 
      (exceedsNoisyThreshold((float)LOW_COUNT_SOFT_THRESHOLD,
                     (float) bp->bsize, (float) LOW_COUNT_SD) == 0)) {
    numLowCount++;
    return(0);
  }

  makeFilterFromBucket(bp);
  if (sfIndex < maxSfIndex) {
    j = sfIndex;
    storedFilters[sfIndex++] = bp;
  }
  else {
    printf("Run out of room for filters!\n");
    exit(1);
  }

  // It is probably wasteful to duplicate bp here, but 
  // at all times, suppressbp will point to a bucket that is missing
  // suppressed users (from the original bucket bp).  suppressbp may
  // be generated several times, depending on the attack scenarios
  // discovered.
  suppressbp = dupBucket(bp);
  childAdded = 0;
  for (i = 0; i < j; i++) {
    numComparisons++;
    bp1 = storedFilters[i];
    compareFullFilters(bp1, bp, &c);
    overlap = (int) ((float) c.overlap * (float) 1.5625);
    if (overlap > NEAR_MATCH_THRESHOLD) {
      // filters suggest that there is a lot of overlap
      numNearMatch++;
      // for every new MtO and OtO combination, create a "composite"
      //     bucket (composed of combinations of children), and check for
      //     sizes are close etc.
      // first check for OtO attack
      nearMatch = checkNearMatchAndTouchNonOverlap(bp, bp1);
      if (nearMatch == 1) {
        // indeed we have an attack situation!  Suppress any
        // non-overlapping users that exceed the attack touch threshold
        suppressbp = getNonOverlapAndSuppress(suppressbp, bp1);
      }
      // now deal with MtO attacks
      // add child link
      if (bp->bsize >= (bp1->bsize + LOW_COUNT_SOFT_THRESHOLD)) {
        // new bucket bp is a parent of former bucket pb1
        addChildLink(bp, i);
        // don't do more until all possible children have been added
        childAdded = 1;
      }
      else if (bp->bsize <= (bp1->bsize - LOW_COUNT_SOFT_THRESHOLD)) {
        // former bucket bp1 is a parent of new bucket pb,
        // check to see if this creates an attack tuple
        suppressbp = checkAllChildCombinations(bp1, bp, suppressbp);
        // When new bucket is child, add link after checking combinations
        addChildLink(bp1, j);
      }
    }
  }
  if (childAdded) {
    suppressbp = checkAllChildCombinations(bp, NULL, suppressbp);
  }
  // At this point, all users that should be suppressed because of the
  // ATTACK threshold have been removed.  But not users that should be
  // suppressed because of the higher ALL threshold.  So remove those
  // users now.
  allbp = makeBucket(suppressbp->bsize);
  allbp->bsize = 0;

  // allbp has allocated space for all of suppressbp, but no users:
  // copy non suppressed users into allbp
  addNonSuppressedUsers(allbp, suppressbp, HT_ALL);
  numAllSuppressed += suppressbp->bsize - allbp->bsize;

  noisyCount = computeNoisyCount(allbp);

  freeBucket(suppressbp);
  freeBucket(allbp);
  // note bp never freed, because stored for later comparisons
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
