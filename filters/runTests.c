#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "./filters.h"

// needed for test_childCombIterator
bucket **storedFilters;
int sfIndex;

FILE *outfile;

main()
{

  outfile = stdout;

  srand48((long int) 10);

  // test_compareFilterPair();
  // runCompareFiltersSpeedTests();
  // test_makeRandomBucket();
  // test_setLevelsBasedOnBucketSize();
  // test_makeFilterFromBucket();
  // test_getNormal();
  // test_combineBuckets();
  // test_getNonOverlap();
  // test_sizesAreClose();
  // test_hsearch();
  // test_createHighTouchTable();
  // test_highTouch();
  // test_getMaxComb();
  // test_childCombIterator();
  // runCompareFullFiltersSpeedTests();
  // test_getNonOverlap3();
  // test_linkedList();
  // test_defineBlocks();
  test_defineCluster();
  // measureLowOverlapBuckets();
  // test_removeChildComb();
  // test_hammerList();
  // test_clusterList();
  // test_initClusterNearMatches();
  // test_defineBarbellChainCluster();
}
