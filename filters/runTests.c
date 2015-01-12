#include <stdio.h>

main()
{
  srand48((long int) 1);

  test_compareFilterPair();
  runCompareFiltersSpeedTests();
  test_makeRandomBucket();
  test_setLevelsBasedOnBucketSize();
  test_makeFilterFromBucket();
}
