#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./clusters.h"

extern int clusters[LARGEST_CLUSTER][MOST_BUCKETS];
extern int clusters_index[LARGEST_CLUSTER];

initClusters()
{
  int i;

  for (i = 0; i < LARGEST_CLUSTER; clusters_index[i++] = 0);
}

/*
 *  Adds '*cluster' to the cluster array of size 'to'.
 *  '*cluster' must have 'to' entries.
 */
addToCluster(int to, int *cluster)
{
  int i;

  if ((clusters_index[to] + to) >= MOST_BUCKETS) {
    // we should have blocked queries by now, so shouldn't happen
    printf("addToCluster() ERROR size exceeded\n");
    exit(1);
  }
  for (i = 0; i < to; i++) {
    clusters[to][clusters_index[to]] = cluster[i];
    clusters_index[to]++;
  }
}

/*
 * removes the cluster in cluster array 'from' at location 'index'
 */
removeFromCluster(int from, int index)
{
  int i, last;

  if (clusters_index[from] == 0) {
    printf("removeFromCluster() ERROR cluster empty (%d)\n", index);
    exit(1);
  }
  if (index >= clusters_index[from]) {
    printf("removeFromCluster() ERROR index too big (%d, %d)\n", index,
                                                    clusters_index[from]);
    exit(1);
  }
  if (index == (clusters_index[from] - from)) {
    // removed cluster is the last one in the array, so do nothing
  }
  else {
    // need to move last cluster into location emptied by removed
    // cluster
    last = clusters_index[from] - from;
    for (i = 0; i < from; i++) {
      clusters[from][index++] = clusters[from][last++];
    }
  }
  // modify index to reflect removed cluster
  clusters_index[from] -= from;
}

/*
 * moves the cluster from the 'from' array at position 'index'
 * to the next bigger array, adding 'bucket' to the cluster in the process
 */
growFromCluster(int from, int index, int bucket)
{
  int new_cluster[MOST_BUCKETS];
  int i;

  if (from >= (LARGEST_CLUSTER-1)) {
    printf("growFromCluster() ERROR cluster too big %d\n", from);
    exit(1);
  }
  // first form the new cluster
  for (i = 0; i < from; i++) {
    new_cluster[i] = clusters[from][index+i];
  }
  new_cluster[i] = bucket;

  // add the cluster to the next array
  addToCluster(from+1, new_cluster);

  // remove the cluster from the previous array
  removeFromCluster(from, index);
}
