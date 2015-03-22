#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./filters.h"

extern bucket *makeRandomBucket(int size);

LIST_HEAD(hammerListHead, hammer_list_entry_t) hammerList;
LIST_HEAD(allClustersHead, cluster_t) allClustersList;

initAllClustersList()
{
  LIST_INIT(&allClustersList);          // Initialize the list.
}

freeCluster(cluster *cp)
{
  bucket *bp;

  while (bp = cp->head.lh_first) {
    removeFromCluster(bp);
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
addToCluster(bucket *new_bp, bucket *prev_bp, int overlap)
{
  int ohist;
  cluster *cp;

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
  }
}

removeFromCluster(bucket *bp)
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

  while (bp = cp2->head.lh_first) {
    removeFromCluster(bp);
    addToClusterList(cp1, bp);
  }
  // transfer the statistics of cp2 into cp1
  for (i = 0; i < CLUSTER_OVERLAP_HISTOGRAM; i++) {
    cp1->histogram[i] += cp2->histogram[i];
  }
  // 
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

printAllClusters()
{
  cluster *cp;
  int i=0;

  for (cp = allClustersList.lh_first; cp != NULL; cp = cp->entry.le_next) {
    printf("****** Cluster %d: %p, size %d *******\n", i++, cp, cp->num);
    printCluster(cp);
  }
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
  removeFromCluster(bp);
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

  printf("test_clusterList() passed\n");
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

