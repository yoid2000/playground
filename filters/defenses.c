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
#define MAX_OVERLAP_HISTOGRAM 11
int overlapHistogram[MAX_OVERLAP_HISTOGRAM];
int numComparisons;
int numPastDigest;
int numCloseSize;
int numSmallOverlap;
int numPutBuckets;
int numLowCount;
int numAdjust;
int numOtMdetect;
float fixedNoiseBias;
int numFixedNoise;
float roundingBias;
int numRounding;
cluster_stats sumCs;

/*
 *  This happens once per attack.
 */
initDefense(int maxBuckets) {
  int i;

  storedFilters = (bucket **) calloc(maxBuckets, sizeof(bucket *));
  sfIndex = 0;
  maxSfIndex = maxBuckets;
  // this hash table much bigger than needed
  initHighTouchTable();
}

/*
 *  This happens once for the whole run
 */
initDefenseStats()
{
  int i;

  for (i = 0; i < MAX_TOUCH_COUNT; i++) {
    numTouches[i] = 0;
  }
  for (i = 0; i < MAX_OVERLAP_HISTOGRAM; i++) {
    overlapHistogram[i] = 0;
  }
  numPutBuckets = 0;
  numLowCount = 0;
  numComparisons = 0;
  numPastDigest = 0;
  numCloseSize = 0;
  numSmallOverlap = 0;
  numAdjust = 0;
  fixedNoiseBias = 0.0;
  numFixedNoise = 0;
  roundingBias = 0.0;
  numRounding = 0;
  initClusterStats(&sumCs);
}

computeDefenseStats(int numRounds, attack_setup *as)
{
  int i, j;

  if (as->defense >= OtO_DEFENSE) {
    fprintf(as->f, "Num Touches Histogram: ");
    for (i = 0; i < MAX_TOUCH_COUNT; i++) {
      if (numTouches[i] != 0) {
        fprintf(as->f, "%d: %.2f ", i, 
               (float)((float)numTouches[i]/(float)numRounds));
      }
    }
    fprintf(as->f, "\nOverlap Histogram: ");
    for (i = 0; i < MAX_OVERLAP_HISTOGRAM; i++) {
      if (overlapHistogram[i] != 0) {
        fprintf(as->f, "%d: %.2f ", i*10, 
               (float)((float)overlapHistogram[i]/(float)numRounds));
      }
    }
    fprintf(as->f, "\n%.2f buckets (%.2f low count), %.2f bucket pairs\n", 
           (float)((float)numPutBuckets/(float)numRounds), 
           (float)((float)numLowCount/(float)numRounds), 
           (float)((float)numComparisons/(float)numRounds));
    if (numComparisons != 0) {
    fprintf(as->f, "%.2f (%d%%) past digest\n", 
           (float)((float)numPastDigest/(float)numRounds), 
            (int)((float)(numPastDigest*100)/(float) numComparisons));
    }
    if (numPastDigest != 0) {
    fprintf(as->f, "%.2f (%d%%) close sizes\n", 
           (float)((float)numCloseSize/(float)numRounds), 
            (int)((float)(numCloseSize*100)/(float) numPastDigest));
    }
    if (numCloseSize != 0) {
    fprintf(as->f, "%.2f (%d%%) small overlaps\n", 
           (float)((float)numSmallOverlap/(float)numRounds), 
            (int)((float)(numSmallOverlap*100)/(float) numCloseSize));
    }
    if (numSmallOverlap != 0) {
    fprintf(as->f, "Average %.2f adjusted users per checked bucket\n",
            (float)((float)numAdjust/(float) numSmallOverlap));
    }
    fprintf(as->f, "Biases: fixed: %.2f (%.2f, %d), rounding: %.2f (%.2f, %d)\n",
            (float)(fixedNoiseBias/(float) numFixedNoise),
            fixedNoiseBias, numFixedNoise,
            (float)(roundingBias/(float) numRounding),
            roundingBias, numRounding);
    fprintf(as->f, "      %.2f Clusters, Size: av %.2f, max %.2f, sd %.2f\n", 
          (float)((float)sumCs.numClusters/(float)numRounds), 
          (float)(sumCs.sizeS.av/(float)numRounds),
          (float)(sumCs.sizeS.max/(float)numRounds), 
          (float)(sumCs.sizeS.sd/(float)numRounds));
    fprintf(as->f, "      Overlap: total %.2f, av %.2f, max %.2f, sd %.2f\n", 
          (float)((float)sumCs.totalClusterOverlap/(float)numRounds), 
          (float)(sumCs.overlapS.av/(float)numRounds), 
          (float)(sumCs.overlapS.max/(float)numRounds), 
          (float)(sumCs.overlapS.sd/(float)numRounds));
    for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
      fprintf(as->f, "          %d: av %.2f, max %.2f, sd %.2f\n", j*10, 
           (float)(sumCs.histS[j].av/(float)numRounds), 
           (float)(sumCs.histS[j].max/(float)numRounds), 
           (float)(sumCs.histS[j].sd/(float)numRounds));
    }

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

/*
 *  This happens once per attack.
 */
endDefense(attack_setup *as) 
{
  cluster_stats cs;

  getAllClustersStats(&cs);
  //printClusterStats(&cs, as->f);
  //printAllClusters();
  addClusterStats(&sumCs, &cs);
  countNumTouches(numTouches, MAX_TOUCH_COUNT);
  freeAllClustersList();
  freeStoredFilters();
  free(storedFilters);
}

float
computeNoisyCount(bucket *bp)
{
  int i;
  float noise, fixedNoise=0.0, roundedNoise=0.0;
  unsigned int fix=0;

  noise = getNormal(0.0, 2.0);	// random noise, SD=2
  fixedNoise = getFixedNormal(bp, 0.0, 1.5);  // fixed noise, SD=1.5
  // update some stats
  fixedNoiseBias += fixedNoise;
  numFixedNoise++;

  noise += fixedNoise;
  // note: no rounding to 5 here.  I figure that it doesn't defeat any
  // attack, and makes it harder to understand what attacks work...
  // rather we round to the nearest integer:
  roundedNoise = nearbyintf(noise);
  // update more stats
  roundingBias += noise - roundedNoise;
  numRounding++;

  noise = roundedNoise;
  // even though I round to an integer, I am never-the-less
  // returning a float here because there are some funny error
  // biases that I don't understand...
  return((float)bp->bsize + noise);
}

/*
 * Doesn't matter which bucket is bp1 or bp2.
 * Returns 1 if buckets are true near matches
 * returns 0 otherwise
 */
int
checkNearMatchAndTouchNonOverlap(bucket *new_bp, 
                                 bucket *old_bp, 
                                 int *adjustment)
{
  bucket *old_nobp=NULL, *old_obp=NULL, *new_nobp=NULL, *new_obp=NULL;

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
      *adjustment += countHighTouch(old_nobp);
      // The non-overlapping users in the new bucket might als be attack
      // victims, so we "remove" them from the new bucket to compensate
      *adjustment -= countHighTouch(new_nobp);
      free(new_obp);
      free(old_obp);
      free(new_nobp);
      free(old_nobp);
      return(1);
    }
    // TODO:  We should be removing the children after we adjust
    // for a given cluster....
  }
  return(0);
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
 * returns 1 if there was a near match, 0 otherwise
 */
int
checkAllChildCombinations(bucket *pbp,     // parent
                          bucket *cbp,     // child, if new bucket is child
                          int *adjustment)     
{
  bucket *cobp;
  child_comb c;
  bucket *nobp;
  int nearMatch = 0;

  if ((cobp = getFirstChildComb(&c, pbp, cbp, storedFilters, sfIndex)) 
                                                                == NULL) {
    // not enough children, so done
    goto finished;
  }
  // In a real implementation, the sizes check would be made before
  // re-querying the buckets.  Here in this simulation, however,
  // getFirstChildComb() builds the composite bucket as well.
  if (cbp) {
    // new bucket is child
    nearMatch = checkNearMatchAndTouchNonOverlap(cobp, pbp, adjustment);
  }
  else {
    // new bucket is parent
    nearMatch = checkNearMatchAndTouchNonOverlap(pbp, cobp, adjustment);
  }
  if (nearMatch == 1) { goto finished; }
  // we keep checking even though we already found one child combination
  // there might be another....
  while (1) {
    if ((cobp = getNextChildComb(&c, pbp, cbp, storedFilters, sfIndex)) 
                                                               == NULL) {
      goto finished;
    }
    // admittedly ugly that I'm repeating this code.  I don't think I
    // built the getFirstChildComb / getNextChildComb interface very
    // smartly...
    if (cbp) {
      // new bucket is child
      nearMatch = checkNearMatchAndTouchNonOverlap(cobp, pbp, adjustment);
    }
    else {
      // new bucket is parent
      nearMatch = checkNearMatchAndTouchNonOverlap(pbp, cobp, adjustment);
    }
    if (nearMatch == 1) { goto finished; }
  }
finished:
  if (nearMatch) {
    // If we get a near match, then remove the matching children
    // Note that if we don't remove the children, then the attacker
    // could simply create lots of dummy children until the
    // MAX_CHILDREN is reached, and then create the real children.
    // In this case, we'd have to detect this and block the new children,
    // which I'd rather not do
    removeChildComb(&c, pbp);
  }
  return(nearMatch);
}

/* NOTES:
1.  Keep a list of barbells.  Look for 2x2 or 3x3 combinations, and
    suppress.  Otherwise, if more than 80 or so barbells without
    suppression, then block.  (This may mean that if I suppress, I can
    remove the corresponding barbells from the list.  But maybe no
    real advantage from removing them.)
2.  Keep a list of barbell/hammer (BH) triples (nunchuk).  Look for
    3x3 double-nunchuk attacks, and 3x2 nunchuk-hammer attacks.
    As far as I know, bigger combinations cannot be made out of nunchuks.  
    Need to confirm.
3.  Keep lists of various sized clusters.  Look for 2x2 attacks.
    If a 2x2 is found (and suppression occurs, probably), then that
    particular 2x2 does not grow into a larger cluster.  However, each
    bucket of the 2x2 can be involved in other clusters.
    If more than 50 or so buckets involved in clusters
    of size 5 or greater, then block.
*/

// A NEAR_MATCH_THRESHOLD of 90 was failing with high numbers of
// children (i.e. 5), because a single match would be missed on most
// clusters.  So we've lowered the threshold to 80.  85 would probably
// be ok in practice too, but this requires more experimentation.
#define NEAR_MATCH_THRESHOLD 80
#define WEAK_MATCH_THRESHOLD 15
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
  int i, j, overlap, ohist;
  compare c;
  bucket *bp1, *lbp, *rbp;
  int childAdded;
  int adjustment=0;
  int nearMatch;

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

  childAdded = 0;
  for (i = 0; i < j; i++) {
    numComparisons++;
    bp1 = storedFilters[i];
    compareFullFilters(bp1, bp, &c);
    overlap = (int) ((float) c.overlap * (float) 1.5625);
    // the following just gathering statistics
    if ((ohist = (int) ((float) overlap / (float) 10)) > 
                                            MAX_OVERLAP_HISTOGRAM) {
      printf("putBucketDefend() ERROR bad ohist %d (%d)\n", ohist, overlap);
      exit(1);
    }
    overlapHistogram[ohist]++;

    if ((as->defense >= MtM_DEFENSE) && (overlap > WEAK_MATCH_THRESHOLD)) {
      // this is a candidate for an MtM attack.  store for later.
      addToCluster(bp, bp1, overlap);
    }
    if (overlap > NEAR_MATCH_THRESHOLD) {
      // filters suggest that there is a lot of overlap
      numPastDigest++;
      // for every new MtO and OtO combination, create a "composite"
      //     bucket (composed of combinations of children), and check for
      //     sizes are close etc.
      // first check for OtO attack
      nearMatch = checkNearMatchAndTouchNonOverlap(bp, bp1, &adjustment);
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
          nearMatch = checkAllChildCombinations(bp1, bp, &adjustment);
          // When new bucket is child, add link after checking combinations
          if (nearMatch == 0) {
            // if there was a near match, then checkAllChildCombinations
            // will have removed the matching children, and we wouldn't want
            // to add this one either
            addChildLink(bp1, j);
          }
        }
      }
    }
  }
  if (as->defense >= MtO_DEFENSE) {
    if (childAdded) {
      nearMatch = checkAllChildCombinations(bp, NULL, &adjustment);
    }
  }

  return(adjustment);
}

float
putBucket(bucket *bp, attack_setup *as)
{
  bucket *allbp;
  float noisyCount;
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
  noisyCount = computeNoisyCount(bp) + (float)adjustment;

  return(noisyCount);
}
