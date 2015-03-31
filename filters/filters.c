#include <stdio.h>
#include <time.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include "./filters.h"

#define OLD_STYLE_FILTER

#ifdef OLD_STYLE_FILTER
#include "./overlap-values.h"
#else
#include "./overlap-values2.h"
#endif

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeBucket(int arg1);
extern int exceedsNoisyThreshold(float arg1, float arg2, float arg3);


initFilter(bucket *bp)
{
  int i, j;

  for (i = 0; i < FILTERS_PER_BUCKET; i++) {
    bp->filters[i].level =0;
    for (j = 0; j < FILTER_LEN; bp->filters[i].filter[j++] = 0);
  }

}

/* 
 * Finds 1) the number of bit positions in common between the
 * two filters, and the unique bits of the first and second
 * filters.  Happily (erroneously) compares two filters of different
 * levels.  Up to calling routing to ensure this doesn't
 * happen.  Never fails.
 */
compareFilterPair(one_filter *of1, one_filter *of2, compare *c)
{
  unsigned int i;

  c->common = 0;  c->first = 0; c->second = 0;
  for (i = 0; i < FILTER_LEN; i++) {
    c->first += __builtin_popcountll(of1->filter[i]);
    c->second += __builtin_popcountll(of2->filter[i]);
    c->common += __builtin_popcountll(of1->filter[i] & of2->filter[i]);
  }
}


/* 
 * Doesn't matter which bucket is bigger coming in.
 */
compareFullFilters(bucket *bp1, bucket *bp2, compare *c)
{
  int i, j;
  int save_i=0, save_j=0, firstTime=1;
  int high, low;
  bucket *bl, *bs;  // large and small
  float frac;

  // force bp1 to be the smaller bucket
  if (bp1->bsize > bp2->bsize) {
    bl = bp1;
    bs = bp2;
  }
  else {
    bl = bp2;
    bs = bp1;
  }
  c->level = 0;    // indicates that no comparison was made
  c->numFirst = bs->bsize;
  c->numSecond = bl->bsize;
  for (i = 0; i < FILTERS_PER_BUCKET; i++) {
    for (j = 0; j < FILTERS_PER_BUCKET; j++) {
      if (bs->filters[i].level == bl->filters[j].level) {
        if (bs->filters[i].level == 0) {goto done;}
        // Ideally, the largest user count is between 300 and 800,
        // and the smallest is least 100.
        // This avoids excessive set bits due to simply large numbers
        // (should consider avoiding this by having a 4th filter????)
        low = bs->bsize >> ((bs->filters[i].level * 2) - 2);
        high = bl->bsize >> ((bl->filters[j].level * 2) - 2);
        if (firstTime == 1) {
          firstTime = 0;
          save_i = i;
          save_j = j;
          if ((high >= 300) && (high <= 800) && (low >= 100)) {goto done;}
        }
        else if ((high >= 300) && (high <= 800) && (low >= 100)) {
          save_i = i;
          save_j = j;
          goto done;
        }
      }
    }
  }
done:
  if (firstTime == 0) {
    c->level = bs->filters[save_i].level;
    c->index1 = save_i;
    c->index2 = save_j;
    compareFilterPair(&(bs->filters[save_i]), &(bl->filters[save_j]), c);
#ifdef OLD_STYLE_FILTER
    if ((i = c->first) >= 800) {
      i = 799;
    }
    if ((j = (c->first - c->common)) >= 256) {
      j = 255;
    }
    // to avoid floating point math in this operation, we use
    // (bs->bsize + (bs->bsize>>1)) instead of (bs->bsize * 1.5)
    if ((bs->bsize <= 2000) && ((bs->bsize + (bs->bsize >> 1)) >= bl->bsize)) {
      c->overlap = partial_overlap_array[i][j];
    }
    else {
      c->overlap = full_overlap_array[i][j];
    }
#else
    if (c->second < c->first) {
      // It can happen that fewer bits are set for the larger (second) filter
      // than the first (smaller) filter, especially if the two buckets are
      // roughly the same size.  Since we only computed the array for the
      // case where the second filter is at least as big as the first, we
      // artificially set the second filter.  This is only used to access the
      // table, so this doesn't mess up the computation.
      c->second = c->first;
    }
    if (c->common <= overlap_array_0[c->first][c->second]) {
      c->overlap = 0;
    }
    else if (c->common <= overlap_array_25[c->first][c->second]) {
      // overlap is somewhere between 0% and 25%.
      frac = ((float)(c->common - overlap_array_0[c->first][c->second]) /
              (float)(overlap_array_25[c->first][c->second] - 
                                    overlap_array_0[c->first][c->second]));
      c->overlap = (int)(frac * 25.0);
    }
    else if (c->common <= overlap_array_50[c->first][c->second]) {
      // overlap is somewhere between 25% and 50%.
      frac = ((float)(c->common - overlap_array_25[c->first][c->second]) /
              (float)(overlap_array_50[c->first][c->second] - 
                                    overlap_array_25[c->first][c->second]));
      c->overlap = 25 + (int)(frac * 25.0);
    }
    else if (c->common <= overlap_array_75[c->first][c->second]) {
      // overlap is somewhere between 50% and 75%.
      frac = ((float)(c->common - overlap_array_50[c->first][c->second]) /
              (float)(overlap_array_75[c->first][c->second] - 
                                    overlap_array_50[c->first][c->second]));
      c->overlap = 50 + (int)(frac * 25.0);
    }
    else {
      // overlap is somewhere between 75% and 100%.  100% overlap means that
      // all of the bits in the first filter are also common bits.  So we 
      // don't need a table lookup.
      frac = ((float)(c->common - overlap_array_75[c->first][c->second]) /
                 (float)(c->first - overlap_array_75[c->first][c->second]));
      c->overlap = 75 + (int)(frac * 25.0);
    }
    // this is a bit stupid, but the old-style filter assumed a range
    // of 0-63, while this new filter assume a range of 0-100.  So we
    // need to adjust.  (If the new-style works well, we'll get rid of
    // this).
    c->overlap = (int)((float)c->overlap / (float) 1.5625);
#endif
  }
}

initOneFilter(one_filter *of)
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

/*
 * sizesAreClose() returns 1 if the sizes of the two buckets are
 * within ~threshold of each other, or within 1/2 the smaller bucket size
 * of each other, whichever is smaller.  Returns 0 otherwise.
 */
int
sizesAreClose(int val1, int val2, int threshold) {
  int large, small;
  int sizeThresh;

  if (val1 > val2) {
    large = val1;
    small = val2;
  }
  else {
    large = val2;
    small = val1;
  }
  sizeThresh = ((int)(small * 0.5) < SIZE_CLOSE_THRESH)?
                     (int)(small * 0.5):threshold;
  return((large - small) < sizeThresh);
}

int
sizesAreCloseBucket(bucket *bp1, bucket *bp2, int threshold)
{
  return(sizesAreClose(bp1->bsize, bp2->bsize, threshold));
}

// bitNum is between 0 and 1023
setFilterBit(one_filter *of, unsigned int bitNum)
{
  unsigned int fi;  // filter index
  uint64_t bitOffset = 1;

  fi = bitNum >> FILTER_SHIFT;
  of->filter[fi] |= bitOffset << (bitNum & FILTER_MASK);
}

makeOneFilter(bucket *bp, int i)
{
  unsigned int mask_presets[MAX_LEVEL] = {
    0,
    0,
    0xc0000000,
    0xf0000000,
    0xfc000000,
    0xff000000,
    0xffc00000,
    0xfff00000,
    0xfffc0000,
    0xffff0000,
    0xffffc000};
  uint64_t mask;
  int j, k;
  unsigned int *lp;

  mask = mask_presets[bp->filters[i].level];
  k = 0;
  lp = bp->list;
  for (j = 0; j < bp->bsize; j++) {
    if ((*lp & mask) == 0) {
      // The original code here was this:
      //     setFilterBit(&(bp->filters[i]), (*lp & 0x3ff));
      // But due to the fact that I was creating buckets based on the
      // value of the last X bits of the user ID (which normally would
      // not be available to the attacker), my filters were screwed up.
      // So now I do this, though in a real implementation with random
      // user IDs, this shouldn't be necessary:
      setFilterBit(&(bp->filters[i]), (*lp ^ (*lp >> 5) ^ (*lp >> 9) ^ 
                                      (*lp >> 13) ^ (*lp >> 23)) & 0x3ff);
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

int
do_sizesAreClose(s1, s2, exp) {
  bucket *bp1, *bp2;
  int fail=0;

  bp1 = makeRandomBucket(s1);
  bp2 = makeRandomBucket(s2);
  if (sizesAreCloseBucket(bp1, bp2, 30) != exp) {
    printf("sizesAreClose Failed, sizes %d and %d\n", s1, s2);
    fail = 1;
  }
  freeBucket(bp1);
  freeBucket(bp2);
  return(fail);
}

test_sizesAreClose()
{
  int size1, size2, expected;
  int fail = 0;

  size1 = 200; size2 = 200; expected = 1;
  fail |= do_sizesAreClose(size1, size2, expected);

  size1 = 200; size2 = 220; expected = 1;
  fail |= do_sizesAreClose(size1, size2, expected);
  fail |= do_sizesAreClose(size2, size1, expected);

  size1 = 200; size2 = 250; expected = 0;
  fail |= do_sizesAreClose(size1, size2, expected);
  fail |= do_sizesAreClose(size2, size1, expected);

  size1 = 30; size2 = 48; expected = 0;
  fail |= do_sizesAreClose(size1, size2, expected);
  fail |= do_sizesAreClose(size2, size1, expected);

  size1 = 30; size2 = 44; expected = 1;
  fail |= do_sizesAreClose(size1, size2, expected);
  fail |= do_sizesAreClose(size2, size1, expected);

  if (!fail) {
    printf("test_sizesAreClose PASSED\n");
  }
}

test_makeFilterFromBucket()
{
  bucket *bp;
  int j;
  unsigned int id;
  unsigned int *lp;
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
  id = 0x80000000;
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
  id = 0x80000000;
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
  id = 0x20000000;
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
runCompareFiltersSpeedTests()
{
  struct timespec start, end;
  unsigned long diff;
  one_filter of1, of2;
  unsigned long i;
  compare c;

  initOneFilter(&of1);
  initOneFilter(&of2);

  //clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    compareFilterPair(&of1, &of2, &c);
  }
  //clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Elapsed time = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Time per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Throughput is %d compares per second\n", (int) (BILLION / (diff / NUM_RUNS)));
}

runCompareFullFiltersSpeedTests()
{
  struct timespec start, end;
  unsigned long diff;
  unsigned long i;
  compare c;
  bucket *bp1, *bp2;

  bp1 = makeRandomBucket(100);
  bp2 = makeRandomBucket(1000);
  makeFilterFromBucket(bp1);
  makeFilterFromBucket(bp2);


  //clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    compareFullFilters(bp1, bp2, &c);
  }
  //clock_gettime(CLOCK_MONOTONIC, &end);

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

  initOneFilter(&of1); initOneFilter(&of2);
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 0, 0, 0, "T0");

  initOneFilter(&of1); initOneFilter(&of2);
  of1.filter[2] = 0x3955;
  of2.filter[7] = 0x294481;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 0, 8, 7, "T1");

  initOneFilter(&of1); initOneFilter(&of2);
  of1.filter[0] = 0x1;
  of2.filter[0] = 0x1;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 1, 1, 1, "T2");

  initOneFilter(&of1); initOneFilter(&of2);
  of1.filter[FILTER_LEN-1] = 0x8000000000000000;
  of2.filter[FILTER_LEN-1] = 0x8000000000000000;
  pass += oneTestCompareFilterPair(&of1, &of2, &c, 1, 1, 1, "T3");

  initOneFilter(&of1); initOneFilter(&of2);
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
