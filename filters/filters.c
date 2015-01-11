#include "filters.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/* 
 * Finds the number of bit positions in common between the
 * two filters.  Happily compares two filters of different
 * type.  Up to calling routing to insure this doesn't
 * happen.  Never fails.
 */
unsigned int
compareFilters(filter *f1, filter *f2) 
{
  unsigned int i;
  unsigned int bits = 0;

  for (i = 0; i < FILTER_LEN; i++) {
    bits += __builtin_popcountll((f1->filter[i] & f2->filter[i]));
  }
  return(bits);
}

initFilter(filter *f)
{
  unsigned int i;

  for (i = 0; i < FILTER_LEN; f->filter[i++] = 0);
}

/* TESTS */


#define BILLION 1000000000L
#define NUM_RUNS 1000000L
runCompareFiltersSpeedTests() {
  struct timespec start, end;
  unsigned long diff;
  filter f1, f2;
  unsigned long i;

  initFilter(&f1);
  initFilter(&f2);

  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    compareFilters(&f1, &f2);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Elapsed time = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Time per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Throughput is %d compares per second\n", (int) (BILLION / (diff / NUM_RUNS)));
}


/*
 * Return 0 if pass, -1 if fail
 */
int
test_compareFilters()
{
  filter f1, f2;
  unsigned int result, expected;
  unsigned char *test;
  unsigned int pass = 0;

  initFilter(&f1);
  initFilter(&f2);
  expected = 0;
  test = "T0";
  if ((result = compareFilters(&f1, &f2)) != expected) {
    pass = -1;
    printf("compareFilters %s FAILED, result is %d, should be %d\n",
        test, result, expected);
  }

  initFilter(&f1);
  initFilter(&f2);
  expected = 0;
  test = "T1";
  f1.filter[2] = 0x3955;
  f2.filter[7] = 0x294481;
  if ((result = compareFilters(&f1, &f2)) != expected) {
    pass = -1;
    printf("compareFilters %s FAILED, result is %d, should be %d\n",
        test, result, expected);
  }

  initFilter(&f1);
  initFilter(&f2);
  expected = 1;
  test = "T2";
  f1.filter[0] = 0x1;
  f2.filter[0] = 0x1;
  if ((result = compareFilters(&f1, &f2)) != expected) {
    pass = -1;
    printf("compareFilters %s FAILED, result is %d, should be %d\n",
        test, result, expected);
  }

  initFilter(&f1);
  initFilter(&f2);
  expected = 1;
  test = "T3";
  f1.filter[FILTER_LEN-1] = 0x8000000000000000;
  f2.filter[FILTER_LEN-1] = 0x8000000000000000;
  if ((result = compareFilters(&f1, &f2)) != expected) {
    pass = -1;
    printf("compareFilters %s FAILED, result is %d, should be %d\n",
        test, result, expected);
  }

  initFilter(&f1);
  initFilter(&f2);
  expected = 6;
  test = "T4";
  f1.filter[FILTER_LEN-1] = 0x8000000000000000;
  f2.filter[FILTER_LEN-1] = 0x8000000000000000;
  f1.filter[0] = 0x1;
  f2.filter[0] = 0x1;
  f1.filter[8] = 0x8000200010004000;
  f2.filter[8] = 0x8000200010004000;
  f1.filter[10] = 0x8000200010004000;
  f2.filter[9] = 0x8000200010004000;
  if ((result = compareFilters(&f1, &f2)) != expected) {
    pass = -1;
    printf("compareFilters %s FAILED, result is %d, should be %d\n",
        test, result, expected);
  }

  return(pass);
}
