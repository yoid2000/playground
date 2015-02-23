#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <search.h>
#include "./filters.h"
#include "./hightouch.h"
#include "./attacks.h"

// externs needed to keep compiler from warning
extern bucket *makeBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);
extern float getNormal(float arg1, float arg2);
extern float getFixedNormal(bucket *arg1, float arg2, float arg3);
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
extern int countHighTouch(bucket *arg1);



bucket **storedFilters;
int sfIndex; 	// index into above list
int maxSfIndex;

#define MAX_TOUCH_COUNT 20
int numTouches[MAX_TOUCH_COUNT];
int numComparisons;
int numPastDigest;
int numCloseSize;
int numSmallOverlap;
int numPutBuckets;
int numLowCount;
int numAdjust;
int numOtMdetect;

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
  numPastDigest = 0;
  numCloseSize = 0;
  numSmallOverlap = 0;
  numAdjust = 0;
}

computeDefenseStats(int numRounds, attack_setup *as)
{
  int i;

  if (as->defense >= OtO_DEFENSE) {
    fprintf(as->f, "Num Touches Histogram:\n");
    for (i = 0; i < MAX_TOUCH_COUNT; i++) {
      if (numTouches[i] != 0) {
        fprintf(as->f, "    %d: %.2f\n", i, 
               (float)((float)numTouches[i]/(float)numRounds));
      }
    }
    fprintf(as->f, "%.2f buckets (%.2f low count), %.2f bucket pairs\n", 
           (float)((float)numPutBuckets/(float)numRounds), 
           (float)((float)numLowCount/(float)numRounds), 
           (float)((float)numComparisons/(float)numRounds));
    fprintf(as->f, "%.2f (%d%%) past digest\n", 
           (float)((float)numPastDigest/(float)numRounds), 
            (int)((float)(numPastDigest*100)/(float) numComparisons));
    fprintf(as->f, "%.2f (%d%%) close sizes\n", 
           (float)((float)numCloseSize/(float)numRounds), 
            (int)((float)(numCloseSize*100)/(float) numPastDigest));
    fprintf(as->f, "%.2f (%d%%) small overlaps\n", 
           (float)((float)numSmallOverlap/(float)numRounds), 
            (int)((float)(numSmallOverlap*100)/(float) numCloseSize));
    fprintf(as->f, "Average %.2f adjusted users per checked bucket\n",
            (float)((float)numAdjust/(float) numSmallOverlap));
  }
}

freeStoredFilters()
{
  int i;

  for (i = 0; i < sfIndex; i++) {
    freeBucket(storedFilters[i]);
  }
  sfIndex = 0;
}

endDefense() 
{
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

  noise = getNormal(0.0, 2.0);	// random noise, SD=2
  noise += getFixedNormal(bp, 0.0, 1.5);  // fixed noise, SD=1.5
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
checkNearMatchAndTouchNonOverlap(bucket *new_bp, bucket *old_bp)
{
  bucket *old_nobp=NULL, *old_obp=NULL, *new_nobp=NULL, *new_obp=NULL;
  int adjustment=0;

  if (sizesAreClose(new_bp, old_bp)) {
    // the exact bucket size (stored with the filter) provides
    // further evidence that this may be an attack.  So now we
    // need to re-query the other bucket and check exact overlap.
    numCloseSize++;
    // Find non-overlapping users (sort and walk):
    sortBucketList(new_bp);
    sortBucketList(old_bp);
    getNonOverlap(new_bp, old_bp, &new_nobp, &old_nobp, &new_obp, &old_obp);
    // Count non-overlapping users in high-touch hash
    if (((old_nobp->bsize + new_nobp->bsize) < SIZE_CLOSE_THRESH) &&
        ((old_nobp->bsize + new_nobp->bsize) > 0)) {
      numSmallOverlap++;
      // the combined non-overlap is small enough that
      // we consider the two buckets to be near matches.

      // touch the overlapping users from both buckets (note that
      // this means that, for the former bucket, I might be touching 
      // users more than once for the same bucket).
      touchNonOverlappingUsers(old_nobp);
      touchNonOverlappingUsers(new_nobp);
      // The non-overlapping users in the former bucket might be attack
      // victims, so we "add" them to the new bucket to compensate
      adjustment += countHighTouch(old_nobp);
      // The non-overlapping users in the new bucket might als be attack
      // victims, so we "remove" them from the new bucket to compensate
      adjustment -= countHighTouch(new_nobp);
      free(new_obp);
      free(old_obp);
      free(new_nobp);
      free(old_nobp);
    }
  }
  return(adjustment);
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
int
checkAllChildCombinations(bucket *pbp,     // parent
                          bucket *cbp)     // child, if new bucket is child
{
  bucket *cobp;
  child_comb c;
  bucket *nobp;
  int adjustment=0;

  if ((cobp = getFirstChildComb(&c, pbp, cbp, storedFilters, sfIndex)) 
                                                                == NULL) {
    // not enough children, so done
    return(adjustment);
  }
  // In a real implementation, the sizes check would be made before
  // re-querying the buckets.  Here in this simulation, however,
  // getFirstChildComb() builds the composite bucket as well.
  if (cbp) {
    // new bucket is child
    adjustment += checkNearMatchAndTouchNonOverlap(cobp, pbp);
  }
  else {
    // new bucket is parent
    adjustment += checkNearMatchAndTouchNonOverlap(pbp, cobp);
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
    if (cbp) {
      // new bucket is child
      adjustment += checkNearMatchAndTouchNonOverlap(cobp, pbp);
    }
    else {
      // new bucket is parent
      adjustment += checkNearMatchAndTouchNonOverlap(pbp, cobp);
    }
  }
  return(adjustment);
}

// the following threshold should catch most though not quite all near
// matching buckets.  Because of added noise, it is not critical
// that we catch all near matches.
#define NEAR_MATCH_THRESHOLD 90
#define WEAK_MATCH_THRESHOLD 30
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
 * returns a possibly-suppressed bucket
 * calling routine must free the returned bucket
 */
int
putBucketDefend(bucket *bp, attack_setup *as) 
{
  LIST_HEAD(listhead, bucket_t) head;
  int i, j, overlap;
  compare c;
  bucket *bp1, *lbp, *rbp;
  int childAdded;
  int adjustment=0;

  countUsers(bp);

  makeFilterFromBucket(bp);
  if (sfIndex < maxSfIndex) {
    j = sfIndex;
    storedFilters[sfIndex++] = bp;
  }
  else {
    printf("Run out of room for filters!\n");
    exit(1);
  }

  if (as->defense >= MtM_DEFENSE) {
    // initialize list of medium overlap buckets
    LIST_INIT(&head);
  }

  childAdded = 0;
  for (i = 0; i < j; i++) {
    numComparisons++;
    bp1 = storedFilters[i];
    compareFullFilters(bp1, bp, &c);
    overlap = (int) ((float) c.overlap * (float) 1.5625);
    if ((as->defense >= MtM_DEFENSE) && (overlap > WEAK_MATCH_THRESHOLD)) {
      // this is a candidate for an MtM attack.  store for later.
      LIST_INSERT_HEAD(&head, bp1, mtm_list);
    }
    if (overlap > NEAR_MATCH_THRESHOLD) {
      // filters suggest that there is a lot of overlap
      numPastDigest++;
      // for every new MtO and OtO combination, create a "composite"
      //     bucket (composed of combinations of children), and check for
      //     sizes are close etc.
      // first check for OtO attack
      adjustment += checkNearMatchAndTouchNonOverlap(bp, bp1);
      if (as->defense >= MtO_DEFENSE) {
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
          adjustment += checkAllChildCombinations(bp1, bp);
          // When new bucket is child, add link after checking combinations
          addChildLink(bp1, j);
        }
      }
    }
  }
  if (as->defense >= MtO_DEFENSE) {
    if (childAdded) {
      adjustment += checkAllChildCombinations(bp, NULL);
    }
  }
  if (as->defense >= MtM_DEFENSE) {
    // go through each pair of overlapping buckets, and see if an MtM
    // attack scenario exists (this I admit is a bit nasty.  It means
    // that we really need to store the full buckets, at least for a
    // while, and process them.)
    for (lbp = head.lh_first; lbp != NULL; lbp = lbp->mtm_list.le_next) {
      for (rbp = head.lh_first; rbp != NULL; rbp = rbp->mtm_list.le_next) {
        if (rbp == lbp) {continue;}
// zzzz
      }
    }
    // clean out the linked list
    while (head.lh_first != NULL) {
      printf("%p\n", head.lh_first);
      LIST_REMOVE(head.lh_first, mtm_list);
    }
  }

  return(adjustment);
}

int
putBucket(bucket *bp, attack_setup *as)
{
  bucket *allbp;
  int noisyCount;
  int adjustment=0;

  numPutBuckets++;

  // do low-count filter early-on.  If fails, then we don't even
  // bother to remember the bucket.   Attacker can't do anything with this.
  if ((bp->bsize <= LOW_COUNT_HARD_THRESHOLD) || 
      (exceedsNoisyThreshold((float)LOW_COUNT_SOFT_THRESHOLD,
                     (float) bp->bsize, (float) LOW_COUNT_SD) == 0)) {
    numLowCount++;
    return(0);
  }

  if (as->defense > BASIC_DEFENSE) {
    // putBucketDefend stores the bp in a data structure for future use.
    // Note that if we don't call putBucketDefend, we still don't bother
    // to free the bucket.  This is intentional and not a problem.
    adjustment = putBucketDefend(bp, as);
    numAdjust += adjustment;
  }
  noisyCount = computeNoisyCount(bp) + adjustment;

  return(noisyCount);
}
