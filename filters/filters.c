#include "./filters.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeBucket(int arg1);

/* 
 * Finds 1) the number of bit positions in common between the
 * two filters, and the unique bits of the first and second
 * filters.  Happily (enormously) compares two filters of different
 * levels.  Up to calling routing to ensure this doesn't
 * happen.  Never fails.
 */
compareFilterPair(one_filter *of1, one_filter *of2, compare *c)
{
  unsigned int i;
  uint64_t anded;

  c->common = 0;  c->first = 0; c->second = 0;
  for (i = 0; i < FILTER_LEN; i++) {
    c->first += __builtin_popcountll(of1->filter[i]);
    c->second += __builtin_popcountll(of2->filter[i]);
    c->common += __builtin_popcountll(of1->filter[i] & of2->filter[i]);
  }
}

/* 
 * Returns 1 if a pair of filters with matching level were
 * found.  Else returns 0.  (Note that we are returning the
 * highest pair of levels, but this might not necessarily be
 * optimal.)
 */
compareFullFilters(bucket *bp1, bucket *bp2, compare *c)
{
  int i, j;
  int save_i=0, save_j=0, firstTime=1;
  int high, low, temp;

  c->level = 0;    // indicates that no comparison was made
  c->numFirst = bp1->bsize;
  c->numSecond = bp2->bsize;
  for (i = 0; i < FILTERS_PER_BUCKET; i++) {
    for (j = 0; j < FILTERS_PER_BUCKET; j++) {
      if (bp1->filters[i].level == bp2->filters[j].level) {
        if (bp1->filters[i].level == 0) {goto done;}
        // Ideally, the largest user count is between 100 and 600
        // This avoids excessive set bits due to simply large numbers
        // (should consider avoiding this by having a 4th filter????)
        high = bp1->bsize >> ((bp1->filters[i].level * 2) - 2);
        low = bp2->bsize >> ((bp2->filters[j].level * 2) - 2);
        if (low > high) {
          temp = low;
          low = high;
          high = temp;
        }
        if (firstTime == 1) {
          firstTime = 0;
          save_i = i;
          save_j = j;
          if ((high >= 100) && (high <= 600) && (low >= 32)) {goto done;}
        }
        else if ((high >= 100) && (high <= 600) && (low >= 32)) {
          save_i = i;
          save_j = j;
          goto done;
        }
      }
    }
  }
done:
  if (firstTime == 0) {
    c->level = bp1->filters[save_i].level;
    compareFilterPair(&(bp1->filters[save_i]), &(bp2->filters[save_j]), c);
  }
}

initFilter(one_filter *of)
{
  unsigned int i;

  for (i = 0; i < FILTER_LEN; of->filter[i++] = 0);
}

setLevelsBasedOnBucketSize(bucket *bp)
{
  int min, max, level;

  if (bp->bsize < F_ONE_MAX) {
    // special case, only one level possible
    bp->filters[0].level = 1;
    bp->filters[1].level = 0;
    bp->filters[2].level = 0;
  } 
  else if (bp->bsize < F_TWO_MAX) {
    // special case, only one level possible
    bp->filters[0].level = 1;
    bp->filters[1].level = 2;
    bp->filters[2].level = 0;
  } 
  else {
    min = F_TWO_MAX;
    max = F_THREE_MAX;
    for (level = 1; level < MAX_LEVEL; level++) {
      if ((bp->bsize >= min) && (bp->bsize < max)) {
        bp->filters[0].level = level;
        bp->filters[1].level = level+1;
        bp->filters[2].level = level+2;
        break;
      }
      min <<= 2;
      max <<= 2;
    }
  }
  // MAX_LEVEL is so high, we'll never get here without
  // setting the levels
}

// bitNum is between 0 and 1023
setFilterBit(one_filter *of, uint64_t bitNum)
{
  uint64_t fi;  // filter index
  uint64_t bitOffset = 1;

  fi = bitNum >> FILTER_SHIFT;
  of->filter[fi] |= bitOffset << (bitNum & FILTER_MASK);
}

makeOneFilter(bucket *bp, int i)
{
  uint64_t mask_presets[MAX_LEVEL] = {
    0,
    0,
    0xc000000000000000,
    0xf000000000000000,
    0xfc00000000000000,
    0xff00000000000000,
    0xffc0000000000000,
    0xfff0000000000000,
    0xfffc000000000000,
    0xffff000000000000,
    0xffffc00000000000,
    0xfffff00000000000,
    0xfffffc0000000000,
    0xffffff0000000000,
    0xffffffc000000000,
    0xfffffff000000000,
    0xfffffffc00000000,
    0xffffffff00000000,
    0xffffffffc0000000,
    0xfffffffff0000000,
    0xfffffffffc000000,
    0xffffffffff000000,
    0xffffffffffc00000,
    0xffffffffff000000};
  uint64_t mask;
  int j, k;
  uint64_t *lp;

  mask = mask_presets[bp->filters[i].level];
  k = 0;
  lp = bp->list;
  for (j = 0; j < bp->bsize; j++) {
    if ((*lp & mask) == 0) {
      setFilterBit(&(bp->filters[i]), (*lp & 0x3ff));
    }
    lp++;
  }
}

makeFilterFromBucket(bucket *bp)
{
  int i;

  setLevelsBasedOnBucketSize(bp);
  for (i = 0; i < FILTERS_PER_BUCKET; i++) {
    if (bp->filters[i].level == 0) {break;}
    makeOneFilter(bp, i);
  }
}

/* TESTS */

test_makeFilterFromBucket()
{
  bucket *bp;
  int j;
  uint64_t id;
  uint64_t *lp;
  int retval = 0;

  bp = makeBucket(32);
  lp = bp->list;
  id = 0;
  for (j = 0; j < bp->bsize; j++) {
    *lp = id++;
    lp++;
  }
  makeFilterFromBucket(bp);
  if ((bp->filters[0].filter[0] != 0xffffffff) ||
      (bp->filters[0].filter[1] != 0)) {
    printf("FAIL1: test_makeFilterFromBucket, %lx, %lx\n", 
           bp->filters[0].filter[0],
           bp->filters[0].filter[1]);
    retval = -1;
  }
  freeBucket(bp);

  bp = makeBucket(32);
  lp = bp->list;
  id = 0;
  for (j = 0; j < bp->bsize; j++) {
    *lp = id;
    id += 4;
    lp++;
  }
  makeFilterFromBucket(bp);
  if ((bp->filters[0].filter[0] != 0x1111111111111111) ||
      (bp->filters[0].filter[1] != 0x1111111111111111) ||
      (bp->filters[0].filter[2] != 0)) {
    printf("FAIL2: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[0].filter[0],
           bp->filters[0].filter[1],
           bp->filters[0].filter[2]);
    retval = -1;
  }
  freeBucket(bp);

  bp = makeBucket(32);
  lp = bp->list;
  id = 0x8000000000000000;
  for (j = 0; j < bp->bsize; j++) {
    *lp = id;
    id += 32;
    lp++;
  }
  makeFilterFromBucket(bp);
  if ((bp->filters[0].filter[0] != 0x0000000100000001) ||
      (bp->filters[0].filter[1] != 0x0000000100000001) ||
      (bp->filters[0].filter[15] != 0x0000000100000001)) {
    printf("FAIL3: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[0].filter[0],
           bp->filters[0].filter[1],
           bp->filters[0].filter[15]);
    retval = -1;
  }
  freeBucket(bp);

  bp = makeBucket(210);
  lp = bp->list;
  id = 0x8000000000000000;
  for (j = 0; j < bp->bsize; j++) {
    *lp = id;
    id += 32;
    lp++;
  }
  makeFilterFromBucket(bp);
  if ((bp->filters[0].filter[0] != 0x0000000100000001) ||
      (bp->filters[0].filter[1] != 0x0000000100000001) ||
      (bp->filters[0].filter[15] != 0x0000000100000001)) {
    printf("FAIL4: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[0].filter[0],
           bp->filters[0].filter[1],
           bp->filters[0].filter[15]);
    retval = -1;
  }
  if ((bp->filters[1].filter[0] != 0) ||
      (bp->filters[1].filter[1] != 0) ||
      (bp->filters[1].filter[15] != 0)) {
    printf("FAIL5: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[1].filter[0],
           bp->filters[1].filter[1],
           bp->filters[1].filter[15]);
    retval = -1;
  }
  freeBucket(bp);


  bp = makeBucket(210);
  lp = bp->list;
  id = 0x2000000000000000;
  for (j = 0; j < bp->bsize; j++) {
    *lp = id;
    id += 32;
    lp++;
  }
  makeFilterFromBucket(bp);
  if ((bp->filters[0].filter[0] != 0x0000000100000001) ||
      (bp->filters[0].filter[1] != 0x0000000100000001) ||
      (bp->filters[0].filter[15] != 0x0000000100000001)) {
    printf("FAIL6: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[0].filter[0],
           bp->filters[0].filter[1],
           bp->filters[0].filter[15]);
    retval = -1;
  }
  if ((bp->filters[1].filter[0] != 0x0000000100000001) ||
      (bp->filters[1].filter[1] != 0x0000000100000001) ||
      (bp->filters[1].filter[15] != 0x0000000100000001)) {
    printf("FAIL7: test_makeFilterFromBucket, %lx, %lx, %lx\n", 
           bp->filters[1].filter[0],
           bp->filters[1].filter[1],
           bp->filters[1].filter[15]);
    retval = -1;
  }
  freeBucket(bp);

  if (retval == 0) {
    printf("PASS: test_makeFilterFromBucket\n\n");
  }
}

oneSetLevelsTest(int bucketSize, int l0, int l1, int l2, unsigned char *test)
{
  bucket *bp;
  int retval = 0;

  bp = makeRandomBucket(bucketSize);
  setLevelsBasedOnBucketSize(bp);
  if ((bp->filters[0].level != l0) || 
      (bp->filters[1].level != l1) ||
      (bp->filters[2].level != l2)) {
    printf("FAIL: test_setLevelsBasedOnBucketSize %s, got %d, %d, %d, should be %d, %d, %d\n", test,
          bp->filters[0].level, bp->filters[1].level, bp->filters[2].level,
          l0, l1, l2);
    retval = -1;
  }
  freeBucket(bp);
  return(retval);
}

test_setLevelsBasedOnBucketSize()
{
  int passed = 0;

  passed += oneSetLevelsTest(35, 1, 0, 0, "T0");
  passed += oneSetLevelsTest(210, 1, 2, 0, "T1");
  passed += oneSetLevelsTest(560, 1, 2, 3, "T2");
  passed += oneSetLevelsTest(5144, 2, 3, 4, "T3");
  passed += oneSetLevelsTest(21030, 3, 4, 5, "T4");

  if (passed == 0) {
    printf("PASSED: test_setLevelsBasedOnBucketSize()\n\n");
  }
}

#define BILLION 1000000000L
#define NUM_RUNS 1000000L
runCompareFiltersSpeedTests() {
  struct timespec start, end;
  unsigned long diff;
  one_filter of1, of2;
  unsigned long i;
  compare c;

  initFilter(&of1);
  initFilter(&of2);

  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    compareFilterPair(&of1, &of2, &c);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Elapsed time = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Time per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Throughput is %d compares per second\n", (int) (BILLION / (diff / NUM_RUNS)));
}

int
oneTestCompareFilterPair(one_filter *of1, one_filter *of2, compare *c,
  int common, int first, int second, unsigned char *test)
{
  compareFilterPair(of1, of2, c);
  if ((c->common != common) || (c->first != first) || (c->second != second)) {
    printf("compareFilterPair %s FAILED, got %d, %d, %d, wanted %d, %d, %d\n",
        test, c->common, c->first, c->second, common, first, second);
    return(-1);
  }
  return(0);
}

/*
 * Return 0 if pass, -1 if fail
 */
test_compareFilterPair()
{
  one_filter of1, of2;
  compare c;
  unsigned int pass = 0;

  initFilter(&of1); initFilter(&of2);
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 0, 0, 0, "T0");

  initFilter(&of1); initFilter(&of2);
  of1.filter[2] = 0x3955;
  of2.filter[7] = 0x294481;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 0, 8, 7, "T1");

  initFilter(&of1); initFilter(&of2);
  of1.filter[0] = 0x1;
  of2.filter[0] = 0x1;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 1, 1, 1, "T2");

  initFilter(&of1); initFilter(&of2);
  of1.filter[FILTER_LEN-1] = 0x8000000000000000;
  of2.filter[FILTER_LEN-1] = 0x8000000000000000;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 1, 1, 1, "T3");

  initFilter(&of1); initFilter(&of2);
  of1.filter[FILTER_LEN-1] = 0x8000000000000000;
  of2.filter[FILTER_LEN-1] = 0x8000000000000000;
  of1.filter[0] = 0x1;
  of2.filter[0] = 0x1;
  of1.filter[8] = 0x8000200010004000;
  of2.filter[8] = 0x8000200010004000;
  of1.filter[10] = 0x8000200010004000;
  of2.filter[9] = 0x8000200010004000;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 6, 10, 10, "T4");

  if (pass == 0) {
    printf("PASSED: test_compareFilterPair()\n\n");
  }
}
