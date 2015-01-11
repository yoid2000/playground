#include <stdio.h>

main()
{
  srand48((long int) 1);

  if (test_compareFilters() == 0) {
    printf("compareFilters tests passed\n");
  }
  runCompareFiltersSpeedTests();
  test_makeRandomBucket();
}
