#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./utilities.h"
#include "./filters.h"

// externs need to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);

#define NUM_SAMPLES 20
testFilterInterpretation()
{
  float oo[NUM_SAMPLES];	//  1:1
  float zz[NUM_SAMPLES];	//  0:0
  float zo[NUM_SAMPLES];	//  0:1
  float os[NUM_SAMPLES];	//  1:*
  mystats ooS, zzS, zoS, osS;
  bucket *bp1, *bp2;
  compare c;
  int bsize1, bsize2, i;
  FILE *fdata;
  unsigned char fileloc[128], filename[128];

  for (bsize1 = 32; bsize1 < 1025; bsize1 *= 2) {
    sprintf(fileloc, "%s", "/home/francis/gnuplot/filterInterpret/");
    sprintf(filename, "%d.data", bsize1);
    strcat(&(fileloc[0]), filename);
    if ((fdata = fopen(fileloc, "w")) == NULL) {
      printf("fopen failed (%s, %s)\n", fileloc, filename); exit(1);
    }
    fprintf(fdata, "# Exclusive buckets\n");
    fprintf(fdata, "# bsize1, bsize2, 1:*, (2sd), 1:1, (2sd), 0:0, (2sd), 0:1, (2sd)\n");
    for (bsize2 = 32; bsize2 < 1025; bsize2 += 32) {
      for (i = 0; i < NUM_SAMPLES; i++) {
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeFilterFromBucket(bp1);
        makeFilterFromBucket(bp2);
        compareFullFilters(bp1, bp2, &c);

        oo[i] = (float) c.common;
        os[i] = (float) c.first;
        zo[i] = (float) (c.second - c.common);
        zz[i] = (float) 1024 + c.common - c.first - c.second;
        if (c.level == 0) {
          printf("sizes %d and %d, levels %d and %d\n", bp1->bsize, bp2->bsize,
               bp1->filters[0].level, bp2->filters[0].level);
        }
        freeBucket(bp1);
        freeBucket(bp2);
      }
      getStats(&ooS, oo, NUM_SAMPLES);
      getStats(&osS, os, NUM_SAMPLES);
      getStats(&zoS, zo, NUM_SAMPLES);
      getStats(&zzS, zz, NUM_SAMPLES);
      fprintf(fdata, "%d %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",
              bsize1, bsize2, 
              osS.av, 2*osS.sd,
              ooS.av, 2*ooS.sd,
              zzS.av, 2*zzS.sd,
              zoS.av, 2*zoS.sd);
    }
    fclose(fdata);
  }
}

#define EXACT 0
#define EXCLUSIVE_100 1
#define EXCLUSIVE_500 2
#define EXCLUSIVE_1000 3
#define EXCLUSIVE_5000 4
#define EXCLUSIVE_10000 5
#define EXCLUSIVE_VARY 6
#define EXCLUSIVE_SAME 7
#define HALF_OVERLAP_SAME 8
#define QUARTER_OVERLAP_SAME 9
#define EIGHTH_OVERLAP_SAME 10

#define MMAX 1000
testMatchingBuckets(int type)
{
  float common[MMAX];
  float first_exclusive[MMAX];
  float second_exclusive[MMAX];
  float first_bsize[MMAX];
  float second_bsize[MMAX];
  float level[MMAX];
  float sizeRatio;
  int	sizeDiff;
  mystats commonStats, firstStats, secondStats;
  mystats bsize1Stats, bsize2Stats, levelStats;
  int i, bsize1, bsize2, temp;
  unsigned char *test;
  compare c;
  bucket *bp1, *bp2;
  FILE *fdata;
  unsigned char *filename;
  unsigned char fileloc[128];

  sprintf(fileloc, "%s", "/home/francis/gnuplot/");
  switch(type) {
    case EXCLUSIVE_SAME:
      filename = "exclusive_same.data";
      break;
    case HALF_OVERLAP_SAME:
      filename = "half_overlap_same.data";
      break;
    case QUARTER_OVERLAP_SAME:
      filename = "quarter_overlap_same.data";
      break;
    case EIGHTH_OVERLAP_SAME:
      filename = "eighth_overlap_same.data";
      break;
    default:
      filename = "default.data";
  }
  
  strcat(&(fileloc[0]), filename);
  if ((fdata = fopen(fileloc, "w")) == NULL) {
    printf("fopen failed (%s, %s)\n", fileloc, filename); exit(1);
  }
  fprintf(fdata, "# c.common, c.first-c.common, c.second-c.common, c.numFirst, c.numSecond, c.level, sizeRatio, sizeDiff\n");

  for (i = 0; i < MMAX; i++) {
    switch(type) {
      case EXACT:
        test = "Exact Match";
        bsize1 = getRandBucketSize(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = dupBucket(bp1);
        break;
      case EXCLUSIVE_100:
        test = "Exclusive Match, Same Size Buckets";
        // Assume enough randomness that collisions never happen
        bsize1 = 100;
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case EXCLUSIVE_500:
        test = "Exclusive Match, Same Size Buckets";
        // Assume enough randomness that collisions never happen
        bsize1 = 500;
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case EXCLUSIVE_1000:
        test = "Exclusive Match, Same Size Buckets";
        // Assume enough randomness that collisions never happen
        bsize1 = 1000;
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case EXCLUSIVE_5000:
        test = "Exclusive Match, Same Size Buckets";
        // Assume enough randomness that collisions never happen
        bsize1 = 5000;
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case EXCLUSIVE_10000:
        test = "Exclusive Match, Same Size Buckets";
        // Assume enough randomness that collisions never happen
        bsize1 = 10000;
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case EXCLUSIVE_SAME:
        test = "Exclusive Match, Same Bucket Size";
        // Assume enough randomness that collisions never happen
        bsize1 = getRandBucketSize(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case HALF_OVERLAP_SAME:
        test = "Half Overlap, Same Bucket Size";
        bsize1 = getRandBucketSize(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>1));
        break;
      case QUARTER_OVERLAP_SAME:
        test = "Quarter Overlap, Same Bucket Size";
        bsize1 = getRandBucketSize(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>2));
        break;
      case EIGHTH_OVERLAP_SAME:
        test = "Quarter Overlap, Same Bucket Size";
        bsize1 = getRandBucketSize(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>3));
        break;
      case EXCLUSIVE_VARY:
        test = "Exclusive Match, Vary Bucket Size";
        // Assume enough randomness that collisions never happen
        bsize1 = getRandBucketSize(200, 5000);
        bsize2 = getRandBucketSize(200, 5000);
        if (bsize1 > bsize2) {
          temp = bsize1;
          bsize1 = bsize2;
          bsize2 = temp;
        }
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
    }
    makeFilterFromBucket(bp1);
    makeFilterFromBucket(bp2);
    compareFullFilters(bp1, bp2, &c);
    common[i] = (float) c.common;
    first_exclusive[i] = (float) (c.first - c.common);
    second_exclusive[i] = (float) (c.second - c.common);
    first_bsize[i] = (float) c.numFirst;
    second_bsize[i] = (float) c.numSecond;
    level[i] = (float) c.level;
    sizeRatio = (float) c.numFirst / (float) c.numSecond;
    sizeDiff = c.numSecond - c.numFirst;
    if (c.level == 0) {
      printf("sizes %d and %d, levels %d and %d\n", bp1->bsize, bp2->bsize,
           bp1->filters[0].level, bp2->filters[0].level);
    }
    fprintf(fdata, "%d %d %d %d %d %d %.2f %d\n",
          c.common, c.first-c.common, c.second-c.common, c.numFirst,
          c.numSecond, c.level, sizeRatio, sizeDiff);
    freeBucket(bp1);
    freeBucket(bp2);
  }
  printf("\nTEST: testMatchingBuckets: %s\n", test);
  getStats(&commonStats, common, MMAX);
  printStats("Common bits in filters", &commonStats, 1);
  getStats(&firstStats, first_exclusive, MMAX);
  printStats("Exclusive bits in first filter", &firstStats, 0);
  getStats(&secondStats, second_exclusive, MMAX);
  printStats("Exclusive bits in second filter", &secondStats, 0);
  getStats(&bsize1Stats, first_bsize, MMAX);
  printStats("Bucket Size First Filter", &bsize1Stats, 0);
  getStats(&bsize2Stats, second_bsize, MMAX);
  printStats("Bucket Size Second Filter", &bsize2Stats, 0);
  getStats(&levelStats, level, MMAX);
  printStats("Level Used", &levelStats, 0);
  fclose(fdata);
}

main()
{
  srand48((long int) 1);

  // test_compareFilterPair();
  // runCompareFiltersSpeedTests();
  // test_makeRandomBucket();
  // test_setLevelsBasedOnBucketSize();
  // test_makeFilterFromBucket();
  // testMatchingBuckets(EXACT);
  // testMatchingBuckets(EXCLUSIVE_100);
  // testMatchingBuckets(EXCLUSIVE_500);
  // testMatchingBuckets(EXCLUSIVE_1000);
  // testMatchingBuckets(EXCLUSIVE_5000);
  // testMatchingBuckets(EXCLUSIVE_10000);
  // testMatchingBuckets(EXCLUSIVE_SAME);
  // testMatchingBuckets(EXCLUSIVE_VARY);
  // testMatchingBuckets(HALF_OVERLAP_SAME);
  // testMatchingBuckets(QUARTER_OVERLAP_SAME);
  // testMatchingBuckets(EIGHTH_OVERLAP_SAME);
  testFilterInterpretation();
}