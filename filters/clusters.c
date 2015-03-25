#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./filters.h"

extern bucket *makeRandomBucket(int size);
extern bucket *makeBucket(int size);
extern bucket *combineBuckets(bucket *bp1, bucket *bp2);

LIST_HEAD(hammerListHead, hammer_list_entry_t) hammerList;
LIST_HEAD(allClustersHead, cluster_t) allClustersList;

/*
 *  The following used by initClusterNearMatches() and
 *  getNextClusterNearMatchComposite();
 */
bucket *nearMatchLeft[MAX_NM_CLUSTERS][MAX_ATTACK_SIDE+1];
bucket *nearMatchRight[MAX_NM_CLUSTERS][MAX_ATTACK_SIDE+1];
int numNearMatch;    // total number of near matches
int nearMatchIndex;  // used when iterating through the matches

int
checkClusterSums(int leftMask, 
          int rightMask, 
          cluster *cp, 
          bucket *bp,
          int *bucketSums,
          int numNearMatch)
{
  int leftSum, rightSum;
  int mask, rightIndex, leftIndex;
  bucket *ibp;

  leftSum = 0; rightSum = 0;
  for (leftIndex = 0, mask = leftMask; mask != 0; leftIndex++, mask >>= 1) {
    if (mask & 0x1) {
      leftSum += bucketSums[leftIndex];
    }
  }
  for (rightIndex = 0, mask = rightMask; mask != 0; rightIndex++, mask >>= 1) {
    if (mask & 0x1) {
      rightSum += bucketSums[rightIndex];
    }
  }
  if (sizesAreClose(rightSum, leftSum, CLUSTER_CLOSE_SIZE)) {
    // ok, we have a potential attack cluster.  Add it to the
    // array
    mask = 1; leftIndex = 0; rightIndex = 0;
    for (ibp = cp->head.lh_first; ibp != NULL; ibp = ibp->entry.le_next) {
      if (ibp == bp) { continue; }  // bp always goes last
      if (mask & leftMask) {
        nearMatchLeft[numNearMatch][leftIndex++] = ibp;
      }
      if (mask & rightMask) {
        nearMatchRight[numNearMatch][rightIndex++] = ibp;
      }
      mask <<= 1;
    }
    if (mask & leftMask) {
      nearMatchLeft[numNearMatch][leftIndex++] = bp;
    }
    if (mask & rightMask) {
      nearMatchRight[numNearMatch][rightIndex++] = bp;
    }
    // set the array ends
    nearMatchLeft[numNearMatch][leftIndex] = NULL;
    nearMatchRight[numNearMatch][rightIndex] = NULL;
    // a little invariant checking...
    if (leftIndex > MAX_ATTACK_SIDE) {
      printf("checkClusterSums ERROR left\n"); exit(1);
    }
    if (rightIndex > MAX_ATTACK_SIDE) {
      printf("checkClusterSums ERROR right\n"); exit(1);
    }
    numNearMatch++;
  }
  return(numNearMatch);
}

bucket *
makeSideComposite(bucket **bpp)
{
  bucket *cobp, *temp;
  int i;

  cobp = makeBucket(0);
  i = 0;
  while (bpp[i]) {
    temp = combineBuckets(cobp, bpp[i]);
    freeBucket(cobp);
    cobp = temp;
    i++;
  }
  return(cobp);
}

/*
 * Returns two composite buckets representing the buckets on the
 * left and the right of a suspected attack cluster.  Caller must
 * free the buckets.
 */
getNextClusterNearMatchComposite(bucket **left_bp, bucket **right_bp)
{
  if (nearMatchIndex == numNearMatch) {
    *left_bp = NULL; *right_bp = NULL;
  }
  else {
    *left_bp = makeSideComposite(nearMatchLeft[nearMatchIndex]);
    *right_bp = makeSideComposite(nearMatchRight[nearMatchIndex]);
  }
  nearMatchIndex++;
}

/*
 * Returns the number of near match clusters, or CLUSTER_TOO_BIG,
 * or CLUSTER_TOO_SMALL, or FOUND_MAX_ATTACK_CLUSTERS.  This last
 * is used to tell the caller that there may be more, unfound,
 * attack clusters.  In this case, the number of returned attack
 * clusters is MAX_NM_CLUSTERS.
 * Sets up the arrays with all near matches.  Overwrites any
 * previous setup arrays, so the caller needs to deal with them
 * before calling initClusterNearMatches() again.
 */
int
initClusterNearMatches(bucket *bp, int action)
{
  cluster *cp;
  bucket *ibp;
  int bucketSums[MAX_CLUSTER_SIZE];
  int maxValue, midValue;
  int i, j;
  int numBuckets;

  nearMatchIndex = 0;
  numNearMatch = 0;
  numBuckets = 0;
  if ((cp = bp->clusterHead) == NULL) {
    printf("initClusterNearMatches() NULL cluster\n");
    exit(1);
  }
  // setup the bucketSums array etc.
  for (ibp = cp->head.lh_first; ibp != NULL; ibp = ibp->entry.le_next) {
    if (ibp == bp) { continue; }  // bp always goes last
    bucketSums[numBuckets++] = ibp->bsize;
    if (numBuckets >= MAX_CLUSTER_SIZE) {
      return(CLUSTER_TOO_BIG);    // would be too expensive to check
    }
  }
  bucketSums[numBuckets++] = bp->bsize;

  if (numBuckets < MIN_CLUSTER_SIZE) {
    return(CLUSTER_TOO_SMALL);    // only interested in 2x2 or bigger
  }

  maxValue = 1 << numBuckets;

  // the following is a very brute-force way of checking all combinations.
  // Not sure how to make it substantially more efficient though...
  if (action == CLUSTER_ADD_NO_JOIN) {
    // If the bucket bp was added to an existing cluster without joining two
    // clusters, then we only need to check combinations that include bp.
    // To do this, we 1) put bp last in the sums list, and 2) start the left
    // counter, which checks all combinations, from the point where the
    // most significant bit, which represents bp, is set.  This way, the
    // most significant bit is set for every checked combination.  The right
    // counter, by contrast, only covers combinations where the most
    // significant bit is not set.
    midValue = maxValue >> 1;
    for (i = midValue; i < maxValue; i++) {
      if ((__builtin_popcount(i) < 2) || 
          (__builtin_popcount(i) > MAX_ATTACK_SIDE)) {
        continue;
      }
      for (j = 1; j < midValue; j++) {
        if ((__builtin_popcount(j) < 2) ||
            (__builtin_popcount(j) > MAX_ATTACK_SIDE)) {
          continue;
        }
        if ((i & j) != 0) { continue; }
        // we have a new combination.  Check its sums.
        numNearMatch = checkClusterSums(i, j, cp, bp, bucketSums, numNearMatch);
        if (numNearMatch == MAX_NM_CLUSTERS) {
          return(FOUND_MAX_ATTACK_CLUSTERS);
        }
      }
    }
  }
  else {
    // when two clusters were joined, then we need to check all the
    // combinations involving both clusters.  Though not the most
    // efficient way of doing it, the following loop simply checks all
    // combinations.
    for (i = 3; i < maxValue; i++) {
      if ((__builtin_popcount(i) < 2) || 
          (__builtin_popcount(i) > MAX_ATTACK_SIDE)) {
        continue;
      }
      for (j = i+1; j < maxValue; j++) {
        if ((__builtin_popcount(j) < 2) ||
            (__builtin_popcount(j) > MAX_ATTACK_SIDE)) {
          continue;
        }
        if ((i & j) != 0) { continue; }
        // we have a new combination.  Check its sums.
        numNearMatch = checkClusterSums(i, j, cp, bp, bucketSums, numNearMatch);
        if (numNearMatch == MAX_NM_CLUSTERS) {
          return(FOUND_MAX_ATTACK_CLUSTERS);
        }
      }
    }
  }
  return(numNearMatch);
}

initAllClustersList()
{
  LIST_INIT(&allClustersList);          // Initialize the list.
}

freeCluster(cluster *cp)
{
  bucket *bp;

  while (bp = cp->head.lh_first) {
    // do WITHOUT_CLUSTER_FREE so that cp isn't free'd in this loop
    removeFromCluster(bp, WITHOUT_CLUSTER_FREE);
  }
  LIST_REMOVE(cp, entry);
  free(cp);
}

freeAllClustersList()
{
  cluster *cp;

  while (cp = allClustersList.lh_first) {
    freeCluster(cp);
  }
}

cluster *
makeCluster()
{
  cluster *cp;
 
  // use calloc cause sets alloc'd memory to zero
  cp = (cluster *) calloc(1, sizeof(cluster));

  LIST_INIT(&(cp->head));                       // Initialize the list.
  LIST_INSERT_HEAD(&allClustersList, cp, entry);
  return(cp);
}

addToClusterList(cluster *cp, bucket *bp)
{
  LIST_INSERT_HEAD(&(cp->head), bp, entry);
  bp->clusterHead = (void *) cp;
  cp->num++;
}

/*
 *  This routine can be called anytime a new bucket has overlap with
 *  an existing bucket.  The existing bucket may or may not already be
 *  in a cluster.  If not, then a new cluster will be created.  The
 *  new bucket also may or may not be in a cluster.  If it is not, then
 *  it will join the cluster of the new bucket.  If it is already in
 *  the same cluster as the existing bucket, then the cluster stats will
 *  be updated, but nothing else happens.  If the new bucket is in a 
 *  different cluster than the existing bucket, then the two clusters
 *  will be joined.
 */
int
addToCluster(bucket *new_bp, bucket *prev_bp, int overlap)
{
  int ohist;
  cluster *cp;
  int retval = CLUSTER_ADD_NO_JOIN;

  if ((cp = prev_bp->clusterHead) == NULL) {
    // these will be the first buckets in a new cluster
    // so make cluster, and add the previous bucket
    cp = makeCluster();
    addToClusterList(cp, prev_bp);
  }
  if (new_bp->clusterHead == NULL) {
    // add new bucket to cluster of previous bucket
    addToClusterList(cp, new_bp);
  }

  // update the cluster stats of the cluster that the new bucket
  // is in
  if (overlap) {
    if ((ohist = (int) ((float)overlap / (float)CLUSTER_OVERLAP_HISTOGRAM)) >= 
                                            CLUSTER_OVERLAP_HISTOGRAM) {
      ohist = CLUSTER_OVERLAP_HISTOGRAM - 1;   // overlap == 100
    }
    cp->histogram[ohist]++;
  }

  // finally, check if there are two clusters that need to be joined
  if (new_bp->clusterHead != prev_bp->clusterHead) {
    joinClusters(prev_bp->clusterHead, new_bp->clusterHead);
    retval = CLUSTER_ADD_JOIN;
  }
  return(retval);
}

/*
 *  Note that removing does not update the histogram in any way.
 *  I don't think this is a problem though, because it only results
 *  in an over-estimate of overlap, and therefore a conservative
 *  decision to block an analyst.  (I think removing will be very
 *  rare, since it implies an attack was found.)
 */
removeFromCluster(bucket *bp, int clusterFree)
{
  cluster *cp;

  cp = (cluster *) bp->clusterHead;

  if (cp == NULL) {
    return;
  }
  if (cp->num <= 0) {
    printf("removeFromCluster ERROR: empty cluster\n");
    exit(1);
  }

  LIST_REMOVE(bp, entry);
  bp->clusterHead = (void *) NULL;
  cp->num--;
  if ((clusterFree == WITH_CLUSTER_FREE) && (cp->num == 0)) {
    LIST_REMOVE(cp, entry);
    free(cp);
  }
}

/*
 * Puts all of the members of cp2 into cp1.
 */
joinClusters(cluster *cp1, cluster *cp2)
{
  bucket *bp;
  int newSize;
  int i;

  newSize = cp1->num + cp2->num;

  // transfer the statistics of cp2 into cp1
  for (i = 0; i < CLUSTER_OVERLAP_HISTOGRAM; i++) {
    cp1->histogram[i] += cp2->histogram[i];
  }
  while (bp = cp2->head.lh_first) {
    // this is ugly, but we do without cluster free here so that the
    // pointer cp2 isn't free'd while in this loop!
    removeFromCluster(bp, WITHOUT_CLUSTER_FREE);
    addToClusterList(cp1, bp);
  }
  freeCluster(cp2);

  if (cp1->num != newSize) {
    printf("joinClusters ERROR bad new size (%d, %d)\n", cp1->num, newSize);
    exit(1);
  }
  if (cp2->num != 0) {
    printf("joinClusters ERROR cp2 not empty (%d)\n", cp2->num);
    exit(1);
  }
}

printCluster(cluster *cp)
{
  bucket *bp;
  int i;

  printf("cluster %p has %d entries:\n", cp, cp->num);
  for (bp = cp->head.lh_first; bp != NULL; bp = bp->entry.le_next) {
    printf("%p, size %d\n", bp, bp->bsize);
  }
  printf("overlap: ");
  for (i = 0; i < CLUSTER_OVERLAP_HISTOGRAM; i++) {
    if (cp->histogram[i]) {
      printf("%d:%d ", i*10, cp->histogram[i]);
    }
  }
  printf("\n");
}

initClusterStats(cluster_stats *csp)
{
  int i;

  csp->numClusters = 0;
  initStats(&(csp->sizeS));
  initStats(&(csp->overlapS));
  csp->totalClusterOverlap = 0;
  for (i = 0; i < CLUSTER_OVERLAP_HISTOGRAM; i++) {
    initStats(&(csp->histS[i]));
  }
}

addClusterStats(cluster_stats *to, cluster_stats *from)
{
  int i;

  to->numClusters += from->numClusters;
  addStats(&(to->sizeS), &(from->sizeS));
  addStats(&(to->overlapS), &(from->overlapS));
  to->totalClusterOverlap += from->totalClusterOverlap;
  for (i = 0; i < CLUSTER_OVERLAP_HISTOGRAM; i++) {
    addStats(&(to->histS[i]), &(from->histS[i]));
  }
}

getAllClustersStats(cluster_stats *csp)
{
  cluster *cp;
  mystats sizeS, overlapS, histS[CLUSTER_OVERLAP_HISTOGRAM];
  int *size, *overlap, *hist[CLUSTER_OVERLAP_HISTOGRAM];
  int j, i, clusterOverlap;

  // first count number of clusters
  csp->numClusters = 0;
  for (cp = allClustersList.lh_first; cp != NULL; cp = cp->entry.le_next) {
    csp->numClusters++;
  }

  // make arrays for stats
  size = (int *) calloc(csp->numClusters, sizeof(int));
  overlap = (int *) calloc(csp->numClusters, sizeof(int));
  for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
    hist[j] = (int *) calloc(csp->numClusters, sizeof(int));
  }

  // fill arrays
  i = 0;
  csp->totalClusterOverlap = 0;
  for (cp = allClustersList.lh_first; cp != NULL; cp = cp->entry.le_next) {
    size[i] = cp->num;
    clusterOverlap = 0;
    for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
      clusterOverlap += ((j*10)+5) * cp->histogram[j];
    }
    for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
      hist[j][i] = cp->histogram[j];
    }
    csp->totalClusterOverlap += clusterOverlap;
    overlap[i] = clusterOverlap;
    i++;
  }

  // compute remaining stats
  getStatsInt(&(csp->sizeS), size, csp->numClusters);
  getStatsInt(&(csp->overlapS), overlap, csp->numClusters);
  for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
    getStatsInt(&(csp->histS[j]), hist[j], csp->numClusters);
  }
}

printClusterStats(cluster_stats *csp, FILE *f)
{
  int j;

  if (f) {
    fprintf(f, "      %d Clusters, Size: av %.2f, max %.2f, sd %.2f\n", 
                           csp->numClusters, csp->sizeS.av,
                           csp->sizeS.max, csp->sizeS.sd);
    fprintf(f, "      Overlap: total %d, av %.2f, max %.2f, sd %.2f\n", 
                        csp->totalClusterOverlap, csp->overlapS.av, 
                        csp->overlapS.max, csp->overlapS.sd);
    fprintf(f, "      Hist: \n");
    for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
      fprintf(f, "          %d: av %.2f, max %.2f, sd %.2f\n", 
                        j*10, csp->histS[j].av, 
                        csp->histS[j].max, csp->histS[j].sd);
    }
  }
  else {
    printf("      %d Clusters, Size: av %.2f, max %.2f, sd %.2f\n", 
                           csp->numClusters, csp->sizeS.av,
                           csp->sizeS.max, csp->sizeS.sd);
    printf("      Overlap: total %d, av %.2f, max %.2f, sd %.2f\n", 
                        csp->totalClusterOverlap, csp->overlapS.av, 
                        csp->overlapS.max, csp->overlapS.sd);
    printf("      Hist: \n");
    for (j = 0; j < CLUSTER_OVERLAP_HISTOGRAM; j++) {
      printf("          %d: av %.2f, max %.2f, sd %.2f\n", 
                        j*10, csp->histS[j].av, 
                        csp->histS[j].max, csp->histS[j].sd);
    }
  }
}

printAllClusters()
{
  cluster *cp;
  cluster_stats cs;
  int i=0;

  getAllClustersStats(&cs);
  printf("\n******  %d Clusters *******\n", cs.numClusters);
  for (cp = allClustersList.lh_first; cp != NULL; cp = cp->entry.le_next) {
    printf("Cluster %d: %p, size %d\n", i++, cp, cp->num);
    printCluster(cp);
  }
  printClusterStats(&cs, NULL);
}


/**************** TESTS ******************/

int test_values[10][20];   // cluster X bucket
int test_count[10];
int test_overlaps[10][CLUSTER_OVERLAP_HISTOGRAM];

checkAllClusters(int error)
{
  bucket *bp;
  cluster *cp;
  int k, i = 0, j = 0;
  int found_size;

  for (cp = allClustersList.lh_first; cp != NULL; cp = cp->entry.le_next) {
    if (test_count[i] != cp->num) {
      printf("test_clusterList() ERROR1 %d (%d, %d)\n", error, test_count[i], cp->num);
      exit(1);
    }
    for (bp = cp->head.lh_first; bp != NULL; bp = bp->entry.le_next) {
      found_size = 0;
      for (j = 0; j < test_count[i]; j++) {
        if (bp->bsize == test_values[i][j]) {
          found_size = 1;
          break;
        }
      }
      if (found_size == 0) {
        printf("test_clusterList() ERROR2 %d (%d)\n", error, bp->bsize);
        exit(1);
      }
    }
    for (k = 0; k < CLUSTER_OVERLAP_HISTOGRAM; k++) {
      if (cp->histogram[k] != test_overlaps[i][k]) {
        printf("test_clusterList() ERROR3 %d (%d, %d, %d)\n", error, k, 
                                      cp->histogram[k], test_overlaps[i][k]);
        exit(1);
      }
    }
    i++;
  }
}

runClusterStress(int print)
{
  bucket *bucketList[1000];
  bucket *bp1, *bp2;
  int i, index;
  cluster_stats cs;

  for (i = 0; i < 1000; i++) {
    bucketList[i] = makeRandomBucket(getRandInteger(50,150));
  }

  i = 0;
  while(1) {
    if (++i > 100) {
      i = 0;
      getAllClustersStats(&cs);
      if (print) printf("\n");
      if (print) printClusterStats(&cs, NULL);
    }
    bp2 = NULL;
    bp1 = bucketList[getRandInteger(0,999)];
    while (1) {
      bp2 = bucketList[getRandInteger(0,999)];
      if (bp2 != bp1) break;
    }
    addToCluster(bp1, bp2, getRandInteger(10,100));

    index = getRandInteger(0,999);
    bp1 = bucketList[index];
    removeFromCluster(bp1, WITH_CLUSTER_FREE);
  }
}

test_clusterList()
{
  cluster *cp, *cp1;
  bucket *bp, *bp1, *bp2;
  int print = 1;
  int error = 0;

  cp = makeCluster();
  if (print) printf("cp = %p\n", cp);
  bp = makeRandomBucket(100);
  if (print) printf("Made bucket %p, size %d\n", bp, bp->bsize);
  addToClusterList(cp, bp);
  bp = makeRandomBucket(200);
  if (print) printf("Made bucket %p, size %d\n", bp, bp->bsize);
  addToClusterList(cp, bp);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 200; test_values[0][test_count[0]++] = 100;
  checkAllClusters(error++);

  if (print) printf("\nRemove %p from cluster\n", bp);
  // in normal operate, we would do "WITH_CLUSTER_FREE", but for this
  // testing, we need to keep cp around
  removeFromCluster(bp, WITHOUT_CLUSTER_FREE);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 100;
  checkAllClusters(error++);

  cp1 = makeCluster();
  if (print) printf("\ncp1 = %p\n", cp1);
  bp = makeRandomBucket(300);
  if (print) printf("Made bucket %p, size %d\n", bp, bp->bsize);
  addToClusterList(cp1, bp);
  bp = makeRandomBucket(400);
  if (print) printf("Made bucket %p, size %d\n", bp, bp->bsize);
  addToClusterList(cp1, bp);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 400; test_values[0][test_count[0]++] = 300;
  test_count[1] = 0; test_values[1][test_count[1]++] = 100;
  checkAllClusters(error++);

  if (print) printf("\nJoin clusters %p and %p:\n", cp, cp1);
  joinClusters(cp, cp1);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 300; test_values[0][test_count[0]++] = 400; test_values[0][test_count[0]++] = 100;
  checkAllClusters(error++);

  if (print) printf("\nFree all clusters:\n");
  freeAllClustersList();
  if (print) printAllClusters();

  if (print) printf("\n");
  bp = makeRandomBucket(150);
  bp1 = makeRandomBucket(151);
  addToCluster(bp, bp1, 15);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 150; 
  test_values[0][test_count[0]++] = 151; 
  test_overlaps[0][1] = 1;
  checkAllClusters(error++);

  addToCluster(bp, bp1, 25);
  if (print) printAllClusters();
  test_overlaps[0][2] = 1;
  checkAllClusters(error++);

  bp1 = makeRandomBucket(152);
  addToCluster(bp, bp1, 35);
  test_values[0][test_count[0]++] = 152; 
  test_overlaps[0][3] = 1;
  if (print) printAllClusters();
  checkAllClusters(error++);

  bp = makeRandomBucket(153);
  bp2 = makeRandomBucket(154);
  addToCluster(bp, bp2, 45);
  if (print) printAllClusters();
  test_count[0] = 0; test_values[0][test_count[0]++] = 153; 
  test_values[0][test_count[0]++] = 154; 
  test_overlaps[0][4] = 1;
  test_overlaps[0][1] = 0;
  test_overlaps[0][2] = 0;
  test_overlaps[0][3] = 0;

  test_count[1] = 0; test_values[1][test_count[1]++] = 150; 
  test_values[1][test_count[1]++] = 151; 
  test_values[1][test_count[1]++] = 152; 
  test_overlaps[1][1] = 1;
  test_overlaps[1][2] = 1;
  test_overlaps[1][3] = 1;
  checkAllClusters(error++);

  addToCluster(bp1, bp2, 55);
  if (print) printAllClusters();
  test_values[0][test_count[0]++] = 150; 
  test_values[0][test_count[0]++] = 151; 
  test_values[0][test_count[0]++] = 152; 
  test_overlaps[0][1] = 1;
  test_overlaps[0][2] = 1;
  test_overlaps[0][3] = 1;
  test_overlaps[0][5] = 1;
  checkAllClusters(error++);

  freeAllClustersList();

  runClusterStress(print);

  printf("test_clusterList() passed\n");
}

doOneCNMTest(int numBuckets, int *sizes, 
             int numWithJoin, int numWithoutJoin, int errorNum)
{
  int i, answer;
  bucket *bp1, *bp2;
  bucket *left_bp, *right_bp;

  bp1 = makeRandomBucket(sizes[0]);
  for (i = 1; i < numBuckets; i++) {
    bp2 = makeRandomBucket(sizes[i]);
    addToCluster(bp1, bp2, 50);
  }
  answer = initClusterNearMatches(bp1, CLUSTER_ADD_NO_JOIN);
  while(1) {
    getNextClusterNearMatchComposite(&left_bp, &right_bp);
    if (left_bp == NULL) { break; }
    printf("Found %d and %d from: ", left_bp->bsize, right_bp->bsize);
    for (i = 0; i < numBuckets; i++) {
      printf("%d, ", sizes[i]);
    }
    printf("\n");
  }
  if (answer != numWithoutJoin) {
    printf("test_initClusterNearMatches ERROR1 %d, got%d, expected %d\n", 
                 errorNum, answer, numWithoutJoin);
    exit(1);
  }
  answer = initClusterNearMatches(bp1, CLUSTER_ADD_JOIN);
  if (answer != numWithJoin) {
    printf("test_initClusterNearMatches ERROR1 %d, got%d, expected %d\n", 
                 errorNum, answer, numWithoutJoin);
    exit(1);
  }
  while(1) {
    getNextClusterNearMatchComposite(&left_bp, &right_bp);
    if (left_bp == NULL) { break; }
    printf("Found %d and %d from: ", left_bp->bsize, right_bp->bsize);
    for (i = 0; i < numBuckets; i++) {
      printf("%d, ", sizes[i]);
    }
    printf("\n");
  }
  freeAllClustersList();
}

test_initClusterNearMatches()
{
  int s[MAX_CLUSTER_SIZE];
  int i, j, k;
  int error=0;
  int testMeasures[MAX_CLUSTER_SIZE];
  int size;
  int numMatches;
  bucket *left_bp, *right_bp;
  bucket *bp[MAX_CLUSTER_SIZE];

  i = 0; s[i++]=600; s[i++]=1000; s[i++]=1200;
  doOneCNMTest(i, s, CLUSTER_TOO_SMALL, CLUSTER_TOO_SMALL, error++);

  i = 0; s[i++]=600; s[i++]=1000; s[i++]=1200; s[i++]=1500;
  doOneCNMTest(i, s, 0, 0, error++);

  i = 0; s[i++]=600; s[i++]=1000; s[i++]=1200; s[i++]=400;
  doOneCNMTest(i, s, 1, 1, error++);

  i = 0; s[i++]=500; s[i++]=501; s[i++]=502; s[i++]=503;
  doOneCNMTest(i, s, 3, 3, error++);

  i = 0; s[i++]=600; s[i++]=1000; s[i++]=1200; s[i++]=1500; s[i++]=1000;
  doOneCNMTest(i, s, 0, 0, error++);

  i = 0; s[i++]=3100; s[i++]=1000; s[i++]=1200; s[i++]=1500; s[i++]=700;
  doOneCNMTest(i, s, 1, 0, error++);
  
  printf("test_initClusterNearMatches unit tests passed, now do measurements\n");
  for (size = MIN_CLUSTER_SIZE; size < MAX_CLUSTER_SIZE; size++) {
    testMeasures[size] = 0;
    for (i = 0; i < 1000; i++) {
      bp[0] = makeRandomBucket(getRandInteger(50, 1000));
      for (j = 1; j < size; j++) {
        bp[j] = makeRandomBucket(getRandInteger(50, 1000));
        addToCluster(bp[j-1], bp[j], 50);
      }
      numMatches = initClusterNearMatches(bp[0], CLUSTER_ADD_NO_JOIN);
      testMeasures[size] += numMatches;
      if (numMatches) {
        printf("%d Composites from: ", numMatches);
        for (j = 0; j < size; j++) {
          printf("%d, ", (bp[j])->bsize);
        }
        printf("\n");
      }
      for (k = 0; k < numMatches; k++) {
        getNextClusterNearMatchComposite(&left_bp, &right_bp);
        if (left_bp == NULL) {
          printf("test_initClusterNearMatches ERROR2 left (%d, %d)\n", 
                                                          k, numMatches);
          exit(1);
        }
        if (right_bp == NULL) {
          printf("test_initClusterNearMatches ERROR2 right (%d, %d)\n", 
                                                          k, numMatches);
          exit(1);
        }
        printf("  Found %d and %d\n", left_bp->bsize, right_bp->bsize);
      }
      getNextClusterNearMatchComposite(&left_bp, &right_bp);
      if (left_bp != NULL) {
        printf("test_initClusterNearMatches ERROR2 left (%d, %d)\n", 
                                                        k, numMatches);
        exit(1);
      }
      if (right_bp != NULL) {
        printf("test_initClusterNearMatches ERROR2 right (%d, %d)\n", 
                                                        k, numMatches);
        exit(1);
      }
      freeAllClustersList();
      for (j = 0; j < size; j++) {
        freeBucket(bp[j]);
      }
    }
    printf("    Cluster size %d, average %.2f attack clusters\n", size,
                          (float)((float) testMeasures[size] / (float) 1000));
  }
}

test_hammerList()
{
  hammer_list_entry *hlep1, *hlep2, *hlep;
  
  LIST_INIT(&hammerList);                       // Initialize the list.
  
  hlep1 = (hammer_list_entry *) malloc(sizeof(hammer_list_entry));
  LIST_INSERT_HEAD(&hammerList, hlep1, entry);
  
  hlep2 = (hammer_list_entry *) malloc(sizeof(hammer_list_entry));
  LIST_INSERT_HEAD(&hammerList, hlep2, entry);
  //LIST_INSERT_AFTER(hlep1, hlep2, entry);
                                          // Forward traversal.
  for (hlep = hammerList.lh_first; hlep != NULL; hlep = hlep->entry.le_next) {
    printf("%p\n", hlep);
  }
  
  while (hammerList.lh_first != NULL) {           // Delete.
    printf("%p\n", hammerList.lh_first);
    LIST_REMOVE(hammerList.lh_first, entry);
  }
}

