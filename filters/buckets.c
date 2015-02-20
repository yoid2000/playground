#include <stdio.h>
#include <time.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>
#include "./filters.h"
#include "./defense.h"

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

  if ((bp = (bucket*) malloc(sizeof(bucket))) == NULL) {
    printf("makeBucket() malloc failed\n");  exit(1);
  }
  bp->sorted = 0;
  if (bsize == 0) {
    // not sure I can calloc something of 0 size, so I put something...
    bp->list = (unsigned int *) calloc(1, sizeof(unsigned int));
  }
  else {
    bp->list = (unsigned int *) calloc(bsize, sizeof(unsigned int));
  }
  bp->bsize = bsize;
  initFilter(bp);
  bp->numChildren = 0;

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


/*
 * The returned bucket must be freed by the calling routine
 */
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
 *  maybe a bit of unnecessary extra work, is there for historical 
 *  reasons, and should probably be fixed.
 *
 *  The input buckets must be sorted.
 *
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

  if (bp1->sorted == 0) {
    sortBucketList(bp1);
  }
  if (bp2->sorted == 0) {
    sortBucketList(bp2);
  }
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

bucket *
getNonOverlap3(bucket *mbp, 	// submitted "middle" (new) bucket (sorted)
      bucket *lbp,      // submitted "left" bucket (sorted)
      bucket *rbp,      // submitted "right" bucket (sorted)
      bucket **r_l_obp,   // exclusive overlap of left and middle buckets
      bucket **r_r_obp,   // exclusive overlap of right and middle buckets
      bucket **r_ml_nobp,   // non-overlap of middle wrt left
      bucket **r_mr_nobp)   // non-overlap of middle wrt right
{
  unsigned int *ml, *ll, *rl, *ml_last, *ll_last, *rl_last;
  bucket *l_obp, *r_obp, *ml_nobp, *mr_nobp;
  unsigned int *l_ol, *r_ol, *ml_nol, *mr_nol;
  int compare_ml, compare_mr, max_size;
  unsigned int rtemp = 0, ltemp = 0;

  if (mbp->sorted == 0) {
    sortBucketList(mbp);
  }
  if (lbp->sorted == 0) {
    sortBucketList(lbp);
  }
  if (rbp->sorted == 0) {
    sortBucketList(rbp);
  }
  // We pessimistically allocate for the worst-case sized buckets.
  max_size = getMax(mbp->bsize, lbp->bsize, rbp->bsize);
  l_obp = makeBucket(max_size);
  r_obp = makeBucket(max_size);
  ml_nobp = makeBucket(max_size);
  mr_nobp = makeBucket(max_size);
  // set up all my list pointers
  ml = mbp->list;
  ml_last = &(mbp->list[mbp->bsize]);
  rl = rbp->list;
  rl_last = &(rbp->list[rbp->bsize]);
  ll = lbp->list;
  ll_last = &(lbp->list[lbp->bsize]);
  l_ol = l_obp->list;
  r_ol = r_obp->list;
  ml_nol = ml_nobp->list;
  mr_nol = mr_nobp->list;
  // and initialize the overlap and non-overlap bucket sizes
  l_obp->bsize = 0;
  r_obp->bsize = 0;
  ml_nobp->bsize = 0;
  mr_nobp->bsize = 0;

  while(1) {
//printf("%p:%d, %p:%d, %p:%d\n", ml, *ml, rl, *rl, ll, *ll);
    if (ml == ml_last) {
      break;
    }
    if (rl == rl_last) {
      // replace the last (biggest) right bucket user with the
      // biggest possible user.  This way, rl is never incremented
      // again, and we don't have to worry about it any more
      rl--;
      if (rtemp != 0) {
        printf("getNonOverlap3() something wrong with rtemp\n");
        exit(1);
      }
      rtemp = *rl;
      *rl = 0x7fffffff;
    }
    if (ll == ll_last) {
      // same for left bucket
      ll--;
      if (ltemp != 0) {
        printf("getNonOvellap3() something wrong with ltemp\n");
        exit(1);
      }
      ltemp = *ll;
      *ll = 0x7fffffff;
    }
    compare_ml = userCmpFunc((const void *)ml, (const void *)ll);
    compare_mr = userCmpFunc((const void *)ml, (const void *)rl);
    if ((compare_ml == 0) && (compare_mr == 0)) {
      // ignore triple matches
      ll++; ml++; rl++;
    }
    else if ((compare_ml < 0) && (compare_mr < 0)) {
      // middle value is alone the min, needs to advance
      *ml_nol = *ml; ml_nol++; ml_nobp->bsize++;
      *mr_nol = *ml; mr_nol++; mr_nobp->bsize++;
      ml++;
    }
    else if ((compare_ml < 0) && (compare_mr == 0)) {
      // middle and right have overlap, and come before left
      *ml_nol = *ml; ml_nol++; ml_nobp->bsize++;
      *r_ol = *ml; r_ol++; r_obp->bsize++;
      ml++; rl++;
    }
    else if ((compare_ml == 0) && (compare_mr < 0)) {
      // middle and left have overlap, and come before right
      *mr_nol = *ml; mr_nol++; mr_nobp->bsize++;
      *l_ol = *ml; l_ol++; l_obp->bsize++;
      ml++; ll++;
    }
    else if ((compare_ml < 0) && (compare_mr > 0)) {
      // no overlap, the right value is  minimum
      // don't care about non-overlap in the right, so advance right value
      rl++;
    }
    else if ((compare_ml > 0) && (compare_mr < 0)) {
      // no overlap, the left value is  minimum
      // don't care about non-overlap in the left, so advance left value
      ll++;
    }
    else if ((compare_ml > 0) && (compare_mr == 0)) {
      // overlap is right, but left is minimum
      // advance left, and pick up the overlap in a future iteration
      ll++;
    }
    else if ((compare_ml == 0) && (compare_mr > 0)) {
      // overlap is left, but right is minimum
      // advance right, and pick up the overlap in a future iteration
      rl++;
    }
    else if ((compare_ml > 0) && (compare_mr > 0)) {
      // no overlap with middle, both left and right are at minimums
      // we don't care about non-overlap between left/right and middle,
      // so just advance left and right values
      rl++; ll++;
    }
  }
  // replace the over-written left and right max entries
  if (rtemp) { *rl = rtemp; }
  if (ltemp) { *ll = ltemp; }
  // finally, set the return pointers
  *r_l_obp = l_obp;
  *r_r_obp = r_obp;
  *r_ml_nobp = ml_nobp;
  *r_mr_nobp = mr_nobp;
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
    index = getRandInteger(0, (userList->bsize-1));
    *lp = userList->list[index];
    lp++;
  }
  return(bp);
}

/*
 *  Returns an array of block definitions that must be freed by caller
 */
blocks *
defineBlocks(int samples, int blocksPerSample)
{
  blocks *bap;
  int i, j, k;

  bap = (blocks *) calloc(samples*blocksPerSample, sizeof(blocks));
  k = 0;
  for (i = 0; i < samples; i++) {
    for (j = 0; j < blocksPerSample; j++) {
      bap[k].sampleNum = i;
      bap[k].childNum = j;
      k++;
    }
  }
  return(bap);
}

getFirstBlockNum(int sampleNum, attack_setup *as)
{
  return(sampleNum * (as->mtmNumBaseBlocks + as->mtmNumExtraBlocks));
}

printMtmCluster(mtm_cluster *mc)
{
  int i, j;

  printf("%d left buckets:\n", mc->numLeft);
  for (i = 0; i < mc->numLeft; i++) {
    printf("    B%d, %d blocks: ", i, mc->leftBuckets[i].numBlocks);
    for (j = 0; j < mc->leftBuckets[i].numBlocks; j++) {
      printf(" %d", mc->leftBuckets[i].blocks[j]);
    }
    printf("\n");
  }
  printf("%d right buckets:\n", mc->numRight);
  for (i = 0; i < mc->numRight; i++) {
    printf("    B%d, %d blocks: ", i, mc->rightBuckets[i].numBlocks);
    for (j = 0; j < mc->rightBuckets[i].numBlocks; j++) {
      printf(" %d", mc->rightBuckets[i].blocks[j]);
    }
    printf("\n");
  }
}

/*
 *  Return 1 if buckets have identical blocks, 0 otherwise
 */
int
bucketBlocksMatch(mtm_bucket *b1, mtm_bucket *b2)
{
  int i, j, match;
  if (b1->numBlocks != b2->numBlocks) {return(0);}
  for (i = 0; i < b1->numBlocks; i++) {
    match = 0;
    for (j = 0; j < b2->numBlocks; j++) {
      if (b1->blocks[i] == b2->blocks[j]) { match = 1; }
    }
    if (match == 0) { return(0); }
  }
  return(1);
}

/*
 * Return 1 if block appears same number of times on both sides
 * 0 otherwise
 */
int
sameOnBothSides(int block, mtm_cluster *mc)
{
  int numLeft = 0, numRight = 0;
  int i, j;

  for (i = 0; i < mc->numLeft; i++) {
    for (j = 0; j < mc->leftBuckets[i].numBlocks; j++) {
      if (mc->leftBuckets[i].blocks[j] == block) {
        numLeft++;
      }
    }
  }
  for (i = 0; i < mc->numRight; i++) {
    for (j = 0; j < mc->rightBuckets[i].numBlocks; j++) {
      if (mc->rightBuckets[i].blocks[j] == block) {
        numRight++;
      }
    }
  }
  if (numLeft == numRight) { return(1); }
  else { return(0); }
}

/*
 * Return 1 if cluster good, 0 if cluster bad
 */
int
clusterPropertiesCheckGood(mtm_cluster *mc)
{
  int i, j, k;

  // make sure no buckets have duplicate blocks
  for (i = 0; i < mc->numLeft; i++) {
    for (j = 0; j < mc->leftBuckets[i].numBlocks; j++) {
      for (k = 0; k < mc->leftBuckets[i].numBlocks; k++) {
        if (k == j) {continue;}
        if (mc->leftBuckets[i].blocks[j] == mc->leftBuckets[i].blocks[k]) {
          return(0);
        }
      }
    }
  }
  for (i = 0; i < mc->numRight; i++) {
    for (j = 0; j < mc->rightBuckets[i].numBlocks; j++) {
      for (k = 0; k < mc->rightBuckets[i].numBlocks; k++) {
        if (k == j) {continue;}
        if (mc->rightBuckets[i].blocks[j] == mc->rightBuckets[i].blocks[k]) {
          return(0);
        }
      }
    }
  }
  
  // make sure every bucket is unique
  for (i = 0; i < mc->numLeft; i++) {
    // compare left-side buckets
    for (j = 0; j < mc->numLeft; j++) {
      if (i == j) {continue;}
      if (bucketBlocksMatch(&(mc->leftBuckets[i]), &(mc->leftBuckets[j]))) {
        return(0);
      }
    }
    // compare left-side with right-side buckets
    for (j = 0; j < mc->numRight; j++) {
      if (bucketBlocksMatch(&(mc->leftBuckets[i]), &(mc->rightBuckets[j]))) {
        return(0);
      }
    }
  }
  for (i = 0; i < mc->numRight; i++) {
    // compare right-side buckets
    for (j = 0; j < mc->numRight; j++) {
      if (i == j) {continue;}
      if (bucketBlocksMatch(&(mc->rightBuckets[i]), &(mc->rightBuckets[j]))) {
        return(0);
      }
    }
  }

  // make sure each side has the same number of each block
  for (i = 0; i < mc->numLeft; i++) {
    for (j = 0; j < mc->leftBuckets[i].numBlocks; j++) {
      if (sameOnBothSides(mc->leftBuckets[i].blocks[j], mc) == 0) {
        return(0);
      }
    }
  }
  for (i = 0; i < mc->numRight; i++) {
    for (j = 0; j < mc->rightBuckets[i].numBlocks; j++) {
      if (sameOnBothSides(mc->rightBuckets[i].blocks[j], mc) == 0) {
        return(0);
      }
    }
  }
  return(1);
}

defineZigZagCluster(mtm_cluster *mc, int sampleNum, attack_setup *as)
{
  int first_bn;
  int lbn, rbn;    // left and right block number
  int lbi=1, rbi=0;  // left and right bucket index
  int i;

  mc->numLeft = as->mtmNumLeftBuckets;
  mc->numRight = as->mtmNumRightBuckets;
  // note numLeft and numRight are identical here
  first_bn = getFirstBlockNum(sampleNum, as);
  lbn = first_bn;
  rbn = lbn;
  // define initial left bucket
  mc->leftBuckets[0].blocks[0] = lbn++;
  mc->leftBuckets[0].numBlocks = 1;
  // define remaining left buckets
  for (i = 1; i < mc->numLeft; i++) {
    mc->leftBuckets[i].blocks[0] = lbn++;
    mc->leftBuckets[i].blocks[1] = lbn++;
    mc->leftBuckets[i].numBlocks = 2;
  }
  // define all but final right buckets
  for (i = 0; i < (mc->numRight - 1); i++) {
    mc->rightBuckets[i].blocks[0] = rbn++;
    mc->rightBuckets[i].blocks[1] = rbn++;
    mc->rightBuckets[i].numBlocks = 2;
  }
  // define final right bucket
  mc->rightBuckets[i].blocks[0] = rbn;
  mc->rightBuckets[i].numBlocks = 1;
}

defineFullCluster(mtm_cluster *mc, int sampleNum, attack_setup *as)
{
}

defineCluster(mtm_cluster *mc, int sampleNum, attack_setup *as)
{
  if (as->mtmType == ZIG_ZAG) {
    defineZigZagCluster(mc, sampleNum, as);
  }
  else if (as->mtmType == FULL_CLUSTER) {
    defineFullCluster(mc, sampleNum, as);
  }
}

bucket *
makeSegregatedBucketFromList(int mask, 
                             int sampleShift,
                             int max_bsize, 
                             int sampleNum,
                             int childNum,
                             unsigned int victim,
                             bucket *userList, 
                             int userListSize)
{
  int value, i, bsize=0;
  unsigned int *lp;
  bucket *bp;

  bp = makeBucket(max_bsize);
  lp = bp->list;

  for (i = 0; i < userListSize; i++) {
    if (bsize >= max_bsize) {
      printf("makeSegregatedBucketFromList ERROR (%d, %d)\n", 
                                                           bsize, max_bsize);
      exit(1);
    }
    value = (sampleNum << sampleShift) | childNum;
    if ((userList->list[i] != victim) && 
        ((userList->list[i] & mask) == value)) {
      *lp = userList->list[i];
      lp++;
      bsize++;
    }
  }
  bp->bsize = bsize;
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
    if (*lp == 0x7fffffff) {
      // reserved value for getNonOverlap3()
      *lp = 0x7ffffffe;
    }
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

bucket *
getNextChildComb(child_comb *c, 
                  bucket *pbp,   // parent
                  bucket *cbp,   // child (if new bucket is child)
                  bucket **sf,
                  int sfMax)
{
  unsigned int i, bit;
  bucket *cobp;     // composite bucket
  bucket *abp;     // bucket to be added to composite
  bucket *temp;

  // we increment the mask first rather than last so that, after
  // return, c->mask is the actual mask used for the composite
  c->mask++;

  if (cbp == NULL) {
    // skip masks with only one child (one bit)
    while (__builtin_popcount(c->mask) < 2) {
      c->mask++;
    }
  }
  if (c->mask > c->maxComb) {
    return(NULL);  // all done
  }
  // build a composite bucket out of the selected children
  // start with an empty dummy bucket to feed into combineBuckets()
  cobp = makeBucket(0);
  bit = 1;
  for (i = 0; i < pbp->numChildren; i++) {
    if (c->mask & bit) {
      if (i >= sfMax) {
        printf("getNextChildComb() Fail 1\n"); exit(1);
      }
      abp = sf[pbp->children[i]];
      temp = combineBuckets(cobp, abp);
      freeBucket(cobp);
      cobp = temp;
    }
    bit <<= 1;
  }
  if (cbp) {
    // The child bucket is the new bucket, but has not yet been linked
    // to the parent's children, so we explicitly add it here.
    temp = combineBuckets(cobp, cbp);
    freeBucket(cobp);
    cobp = temp;
  }

  return(cobp);
}

bucket *
getFirstChildComb(child_comb *c, 
                  bucket *pbp,     // parent
                  bucket *cbp,     // child (if new bucket is child)
                  bucket **sf,
                  int sfMax)
{
  if ((pbp->numChildren == 0) ||
      ((cbp == NULL) && (pbp->numChildren < 2))) {
    return(NULL);      // nothing to compare
  }
  // set a mask with a '1' for every child
  c->maxComb = getMaxComb(pbp->numChildren);
  // initialize the mask to zero
  // because getNextChildComb() starts by incrementing the mask
  c->mask = 0;
  return(getNextChildComb(c, pbp, cbp, sf, sfMax));
}

/*********** TESTS **********/

test_defineZigZagCluster()
{
  mtm_cluster mc;
  attack_setup as;
  int numSamples, sn, fn, nb;
  blocks *block_array;

  numSamples = 20;
  as.mtmNumLeftBuckets = 2;
  as.mtmNumRightBuckets = 2;
  as.mtmNumBaseBlocks = (as.mtmNumLeftBuckets * 2) - 1;
  as.mtmNumExtraBlocks = 0;
  block_array = defineBlocks(numSamples, 
                             (as.mtmNumBaseBlocks + as.mtmNumExtraBlocks));
  sn = 0;
  nb = as.mtmNumBaseBlocks + as.mtmNumExtraBlocks;
  defineZigZagCluster(&mc, sn, &as);
  if (mc.numLeft != 2) {fn = 100; goto fail;}
  if (mc.numRight != 2) {fn = 101; goto fail;}
  if (mc.leftBuckets[0].numBlocks != 1) {fn = 102; goto fail;}
  if (mc.leftBuckets[0].blocks[0] != (nb*sn)) {fn = 103; goto fail;}
  if (mc.leftBuckets[1].numBlocks != 2) {fn = 104; goto fail;}
  if (mc.leftBuckets[1].blocks[0] != (nb*sn)+1) {fn = 105; goto fail;}
  if (mc.leftBuckets[1].blocks[1] != (nb*sn)+2) {fn = 106; goto fail;}
  if (mc.rightBuckets[0].numBlocks != 2) {fn = 107; goto fail;}
  if (mc.rightBuckets[0].blocks[0] != (nb*sn)) {fn = 108; goto fail;}
  if (mc.rightBuckets[0].blocks[1] != (nb*sn)+1) {fn = 109; goto fail;}
  if (mc.rightBuckets[1].numBlocks != 1) {fn = 110; goto fail;}
  if (mc.rightBuckets[1].blocks[0] != (nb*sn)+2) {fn = 111; goto fail;}
  if (clusterPropertiesCheckGood(&mc) != 1) {fn = 118; goto fail;}
  // make cluster bad, and check again
  mc.rightBuckets[1].blocks[0] = (nb*sn)+4;
  if (clusterPropertiesCheckGood(&mc) == 1) {fn = 119; goto fail;}
  
  free(block_array);

  as.mtmNumLeftBuckets = 3;
  as.mtmNumRightBuckets = 3;
  as.mtmNumBaseBlocks = (as.mtmNumLeftBuckets * 2) - 1;
  block_array = defineBlocks(numSamples, 
                             (as.mtmNumBaseBlocks + as.mtmNumExtraBlocks));
  sn = 2;
  nb = as.mtmNumBaseBlocks + as.mtmNumExtraBlocks;
  defineZigZagCluster(&mc, sn, &as);
  if (mc.numLeft != 3) {fn = 200; goto fail;}
  if (mc.numRight != 3) {fn = 201; goto fail;}
  if (mc.leftBuckets[0].numBlocks != 1) {fn = 202; goto fail;}
  if (mc.leftBuckets[0].blocks[0] != (nb*sn)) {fn = 203; goto fail;}
  if (mc.leftBuckets[1].numBlocks != 2) {fn = 204; goto fail;}
  if (mc.leftBuckets[1].blocks[0] != (nb*sn)+1) {fn = 205; goto fail;}
  if (mc.leftBuckets[1].blocks[1] != (nb*sn)+2) {fn = 206; goto fail;}
  if (mc.leftBuckets[2].numBlocks != 2) {fn = 212; goto fail;}
  if (mc.leftBuckets[2].blocks[0] != (nb*sn)+3) {fn = 213; goto fail;}
  if (mc.leftBuckets[2].blocks[1] != (nb*sn)+4) {fn = 214; goto fail;}
  if (mc.rightBuckets[0].numBlocks != 2) {fn = 207; goto fail;}
  if (mc.rightBuckets[0].blocks[0] != (nb*sn)) {fn = 208; goto fail;}
  if (mc.rightBuckets[0].blocks[1] != (nb*sn)+1) {fn = 209; goto fail;}
  if (mc.rightBuckets[1].numBlocks != 2) {fn = 215; goto fail;}
  if (mc.rightBuckets[1].blocks[0] != (nb*sn)+2) {fn = 216; goto fail;}
  if (mc.rightBuckets[1].blocks[1] != (nb*sn)+3) {fn = 217; goto fail;}
  if (mc.rightBuckets[2].numBlocks != 1) {fn = 210; goto fail;}
  if (mc.rightBuckets[2].blocks[0] != (nb*sn)+4) {fn = 211; goto fail;}
  if (clusterPropertiesCheckGood(&mc) != 1) {fn = 218; goto fail;}
  // put two identical blocks in same bucket
  mc.rightBuckets[1].blocks[1] = (nb*sn)+2;
  if (clusterPropertiesCheckGood(&mc) == 1) {fn = 219; goto fail;}

  printf("test_defineZigZigCluster() passed\n");
  printMtmCluster(&mc);
  return;
fail:
  printf("test_defineZigZigCluster() FAIL %d\n", fn);
  printMtmCluster(&mc);
}



test_defineBlocks()
{
  blocks *block_array;
  int i, j, k;

  block_array = defineBlocks(3, 4);
  k = 0;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 4; j++) {
      if ((block_array[k].sampleNum != i) ||
          (block_array[k].childNum != j)) {
        printf("test_defineBlocks() failed\n");
        exit(1);
      }
      k++;
    }
  }
  printf("test_defineBlocks() passed\n");
  free(block_array);
}

test_linkedList()
{
  LIST_HEAD(listhead, bucket_t) head;
  bucket *bp1, *bp2, *bp;
  
  LIST_INIT(&head);                       /* Initialize the list. */
  
  bp1 = makeRandomBucket(100);
  LIST_INSERT_HEAD(&head, bp1, mtm_list);
  
  bp2 = makeRandomBucket(200);
  LIST_INSERT_HEAD(&head, bp2, mtm_list);
  //LIST_INSERT_AFTER(bp1, bp2, mtm_list);
                                          /* Forward traversal. */
  for (bp = head.lh_first; bp != NULL; bp = bp->mtm_list.le_next) {
    printf("%p %d\n", bp, bp->bsize);
  }
  
  while (head.lh_first != NULL) {           /* Delete. */
    printf("%p\n", head.lh_first);
    LIST_REMOVE(head.lh_first, mtm_list);
  }
}

/*
test_linkedList()
{
  LIST_HEAD(listhead, bucket) head;
  struct listhead *headp;
  struct test_bucket *bp1;

  LIST_INIT(&head);
  bp1 = malloc(sizeof struct test_bucket);
  LIST_INSERT_HEAD(&head, bp1, mtm_list);
  bp1 = malloc(sizeof struct test_bucket);
  LIST_INSERT_HEAD(&head, bp1, mtm_list);
  bp1 = malloc(sizeof struct test_bucket);
  LIST_INSERT_HEAD(&head, bp1, mtm_list);

  for (bp1 = head.lh_first; bp1 != NULL; bp1 = bp1->mtm_list.le_next) {
    printf("bp1 = %p\n", bp1);
  }
  while ((bp1 = head.lh_first) != NULL) {
    LIST_REMOVE(head.lh_first, mtm_list);
    printf("Freeing %p\n", bp1);
    freeBucket(bp1);
  }
}
*/

int numCCChecks=0;

checkUsersInComposite(bucket *bp, bucket *cobp, int testNum)
{
  int j, k, found;

  // make sure all the users in the child are in the composite
  for (j = 0; j < bp->bsize; j++) {
    found = 0;
    for (k = 0; k < cobp->bsize; k++) {
      numCCChecks++;
      if (cobp->list[k] == bp->list[j]) {
        found = 1; break;
      }
    }
    if (found == 0) {
      printf("checkComposite FAIL F1%d (%d, %d)\n", testNum, j, k);
      exit(1);
    }
  }
}

checkComposite(unsigned int mask,
               bucket *cobp, 
               bucket *pbp,      // parent
               bucket *cbp,      // child (if new is not parent)
               bucket **sf, 
               int sfMax, 
               int testNum)
{
  int i;
  unsigned int bit = 1;
  int bsize = 0;
  bucket *bp;

  for (i = 0; 1; i++) {
    if (bit > mask) { break; }
    if (bit & mask) {
      bp = sf[pbp->children[i]];
      bsize += bp->bsize;
      checkUsersInComposite(bp, cobp, testNum);
    }
    bit <<= 1;
  }
  if (cbp) {
    bsize += cbp->bsize;
    checkUsersInComposite(cbp, cobp, testNum);
  }
  if (bsize != cobp->bsize) {
    printf("checkComposite FAIL F2%d (bsize = %d, cobp->bsize = %d, mask = %d)\n", 
           testNum, bsize, cobp->bsize, mask);
    exit(1);
  }
}

test_childCombIterator()
{
  child_comb c;
  int i, j;
  bucket *nbp;             // new bucket
  bucket *pbp;             // parent bucket (when new bucket is child)
  bucket *cobp;            // composite bucket
  bucket **sf = NULL;      // stored filters
  int sfMax = 10;
  int ncr, n;

  sf = (bucket **) calloc(sfMax, sizeof(bucket *));
  for (i = 0; i < sfMax; i++) {
    sf[i] = makeRandomBucket(50 + (i*50));
  }

  // first test case where the new bucket nbp is the parent:
  nbp = sf[0];
  nbp->numChildren = 0;
  if ((cobp = getFirstChildComb(&c, nbp, NULL, sf, sfMax)) != NULL) {
    printf("test_childCombIterator FAIL F1!\n"); exit(1);
  }

  nbp->numChildren++;
  nbp->children[nbp->numChildren-1] = nbp->numChildren;
  if ((cobp = getFirstChildComb(&c, nbp, NULL, sf, sfMax)) != NULL) {
    printf("test_childCombIterator FAIL F2!\n"); exit(1);
  }

  for (j = 0; j <= 4; j++) {
    nbp->numChildren++;
    nbp->children[nbp->numChildren-1] = nbp->numChildren;
    if ((cobp = getFirstChildComb(&c, nbp, NULL, sf, sfMax)) == NULL) {
      printf("test_childCombIterator FAIL F3%d!\n", j); exit(1);
    }
    if (c.mask != 3) {
      printf("test_childCombIterator FAIL F5%d!\n", j); exit(1);
    }
    checkComposite(c.mask, cobp, nbp, NULL, sf, sfMax, 4*10);
    for (i = 0; 1; i++) {
      if ((cobp = getNextChildComb(&c, nbp, NULL, sf, sfMax)) == NULL) {
        break;
      }
      checkComposite(c.mask, cobp, nbp, NULL, sf, sfMax, (4*10)+i);
    }
    n = nbp->numChildren;
    ncr = pow(2, n) - n - 1;
    if (ncr != (i+1)) {
      printf("test_childCombIterator FAIL F6%d! (%d, %d, %d)\n", j, n, ncr, i); 
      exit(1);
    }
  }

  // now test case where the new bucket nbp is a child:
  nbp = makeRandomBucket(25);
  pbp = sf[0];
  pbp->numChildren = 0;
  if ((cobp = getFirstChildComb(&c, pbp, nbp, sf, sfMax)) != NULL) {
    printf("test_childCombIterator FAIL F11!\n"); exit(1);
  }

  for (j = 0; j <= 5; j++) {
    pbp->numChildren++;
    pbp->children[pbp->numChildren-1] = pbp->numChildren;
    if ((cobp = getFirstChildComb(&c, pbp, nbp, sf, sfMax)) == NULL) {
      printf("test_childCombIterator FAIL F31%d!\n", j); exit(1);
    }
    if (c.mask != 1) {
      printf("test_childCombIterator FAIL F51%d!\n", j); exit(1);
    }
    checkComposite(c.mask, cobp, pbp, nbp, sf, sfMax, 8*10);
    for (i = 0; 1; i++) {
      if ((cobp = getNextChildComb(&c, pbp, nbp, sf, sfMax)) == NULL) {
        break;
      }
      checkComposite(c.mask, cobp, pbp, nbp, sf, sfMax, (8*10)+i);
    }
    n = pbp->numChildren;
    ncr = pow(2, n) - 1;
    if (ncr != (i+1)) {
      printf("test_childCombIterator FAIL F61%d! (%d, %d, %d)\n", j, n, ncr, i); 
      exit(1);
    }
  }

  printf("test_childCombIterator passed (%d checks).\n", numCCChecks);
}

check_bsizes(int testNum, bucket *lobp, int lo, bucket *robp, int ro, bucket *lnobp, int lno, bucket *rnobp, int rno)
{
  if (lobp->bsize != lo) {
    printf("check_bsizes() ERROR%d bad lobp->bsize %d, expect %d,%d,%d,%d\n", testNum, lobp->bsize, lo, ro, lno, rno); exit(1);
  }
  if (robp->bsize != ro) {
    printf("check_bsizes() ERROR%d bad robp->bsize %d, expect %d,%d,%d,%d\n", testNum, robp->bsize, lo, ro, lno, rno); exit(1);
  }
  if (lnobp->bsize != lno) {
    printf("check_bsizes() ERROR%d bad lnobp->bsize %d, expect %d,%d,%d,%d\n", testNum, lnobp->bsize, lo, ro, lno, rno); exit(1);
  }
  if (rnobp->bsize != rno) {
    printf("check_bsizes() ERROR%d bad rnobp->bsize %d, expect %d,%d,%d,%d\n", testNum, rnobp->bsize, lo, ro, lno, rno); exit(1);
  }
}

putUval(unsigned int uval, bucket *bp) {
  int i, index;

  index = getRandInteger(0, bp->bsize-2);
  while(1) {
    if (bp->list[index] == 0) {
      bp->list[index] = uval;
      break;
    }
    else {
      if (++index >= bp->bsize) { index = 0; }
    }
  }
}

do_getNonOverlap3(int msize, int lsize, int rsize, int lolap, int rolap, int bolap) {
  bucket *lbp, *rbp, *mbp;
  bucket *lobp, *robp, *lnobp, *rnobp;
  int i;
  unsigned int uval;

  if ((lolap + rolap + bolap) > msize) {
    printf("do_getNonOverlap3() pick better values 1 %d, %d, %d, %d\n", 
                   lolap, rolap, bolap, msize);  exit(1);
  }
  if ((lolap + bolap) > lsize) {
    printf("do_getNonOverlap3() pick better values 2 %d, %d, %d\n", 
                   lolap, bolap, lsize);  exit(1);
  }
  if ((rolap + bolap) > rsize) {
    printf("do_getNonOverlap3() pick better values 3 %d, %d, %d\n", 
                   rolap, bolap, rsize);  exit(1);
  }

  lbp = makeBucket(lsize);
  rbp = makeBucket(rsize);
  mbp = makeBucket(msize);

  for (i = 0; i < lolap; i++) {
    uval = lrand48();
    putUval(uval, lbp);
    putUval(uval, mbp);
  }
  for (i = 0; i < rolap; i++) {
    uval = lrand48();
    putUval(uval, rbp);
    putUval(uval, mbp);
  }
  for (i = 0; i < bolap; i++) {
    uval = lrand48();
    putUval(uval, rbp);
    putUval(uval, lbp);
    putUval(uval, mbp);
  }
  for (i = 0; i < rbp->bsize; i++) {
    if (rbp->list[i] == 0) { rbp->list[i] = lrand48(); }
  }
  for (i = 0; i < mbp->bsize; i++) {
    if (mbp->list[i] == 0) { mbp->list[i] = lrand48(); }
  }
  for (i = 0; i < lbp->bsize; i++) {
    if (lbp->list[i] == 0) { lbp->list[i] = lrand48(); }
  }
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(10, lobp, lolap, robp, rolap, lnobp, (msize-lolap-bolap), rnobp, (msize-rolap-bolap));
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

}

test_getNonOverlap3() {
  bucket *lbp, *rbp, *mbp;
  bucket *lobp, *robp, *lnobp, *rnobp;
  unsigned int *lp;
  int i;

  lbp = makeBucket(4);
  rbp = makeBucket(4);
  mbp = makeBucket(8);
  lp = lbp->list; *lp++ = 15; *lp++ = 12; *lp++ = 18; *lp++ = 15;
  lp = rbp->list; *lp++ = 25; *lp++ = 22; *lp++ = 28; *lp++ = 25;
  lp = mbp->list; *lp++ = 35; *lp++ = 32; *lp++ = 38; *lp++ = 35;
  *lp++ = 45; *lp++ = 42; *lp++ = 48; *lp++ = 45;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(1, lobp, 0, robp, 0, lnobp, 8, rnobp, 8);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  lp = lbp->list; *lp++ = 55; *lp++ = 52; *lp++ = 58; *lp++ = 55;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(2, lobp, 0, robp, 0, lnobp, 8, rnobp, 8);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  lp = rbp->list; *lp++ = 65; *lp++ = 62; *lp++ = 68; *lp++ = 65;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(3, lobp, 0, robp, 0, lnobp, 8, rnobp, 8);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  lp = rbp->list; *lp++ = 15; *lp++ = 22; *lp++ = 68; *lp++ = 65;
  lp = lbp->list; *lp++ = 16; *lp++ = 20; *lp++ = 58; *lp++ = 55;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(4, lobp, 0, robp, 0, lnobp, 8, rnobp, 8);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  lp = rbp->list; *lp++ = 15; *lp++ = 22; *lp++ = 32; *lp++ = 65;
  lp = lbp->list; *lp++ = 15; *lp++ = 20; *lp++ = 48; *lp++ = 65;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(5, lobp, 1, robp, 1, lnobp, 7, rnobp, 7);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  lp = rbp->list; *lp++ = 35; *lp++ = 22; *lp++ = 32; *lp++ = 65;
  lp = lbp->list; *lp++ = 15; *lp++ = 20; *lp++ = 48; *lp++ = 35;
  getNonOverlap3(mbp, lbp, rbp, &lobp, &robp, &lnobp, &rnobp);
  check_bsizes(6, lobp, 1, robp, 1, lnobp, 7, rnobp, 7);
  freeBucket(lobp); freeBucket(robp); freeBucket(lnobp); freeBucket(rnobp);

  for (i = 0; i < 1000; i++) {
    do_getNonOverlap3(getRandInteger(200,300),
                      getRandInteger(100,200),
                      getRandInteger(100,200),
                      getRandInteger(0,20),
                      getRandInteger(0,20),
                      getRandInteger(0,20));
  }

  do_getNonOverlap3(200, 300, 90, 200, 0, 0);
  do_getNonOverlap3(200, 300, 90, 0, 0, 90);
  do_getNonOverlap3(200, 300, 300, 0, 0, 200);
  do_getNonOverlap3(200, 300, 300, 50, 50, 100);

  printf("test_getNonOverlap3() passed\n");
}

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
