
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "./utilities.h"
#include "./filters.h"

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);
extern float getRandFloat(float arg1, float arg2);

#define MAX_SAMPLES 16
typedef struct mysamples_t {
  unsigned char samples[MAX_SAMPLES]; 
} mysamples;

#define MAX_VALUE 1024
#define NUM_LEVELS 1
// Put these here (global) cause won't go on stack...
mysamples values[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
mysamples bsize1Samples[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
mysamples levelSamples[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
unsigned char indices[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
unsigned char overlapDiff[101][20];
unsigned char overlapIndex[101];

computeOverlapValues(int maxBucketSize)
{
  bucket *bp1, *bp2;
  compare c;
  int bsize1, bsize2, i, j, k, overlap, index;
  int numFullEntries = 0;
  int numNoLevel = 0;
  int outOfBounds = 0;
  int diffLevel;
  int computedOverlap;
  int absNumOverlap, badOverlap = 0;
  FILE *foo[NUM_LEVELS], *scatter, *fDiff;
  unsigned char fileloc[128];
  float sizeRatio, skew;
  mystats valuesS[NUM_LEVELS], bsize1S[NUM_LEVELS], levelS[NUM_LEVELS];
  mystats diffS;


  for (i = 0; i < MAX_VALUE; i++) {
    for (j = 0; j < MAX_VALUE; j++) {
      for (k = 0; k < NUM_LEVELS; k++) {
      indices[k][i][j] = 0;
      }
    }
  }
  for (i = 0; i < 101; i++) {
    overlapIndex[i] = 0;
  }

  sprintf(fileloc, "/home/francis/gnuplot/computeOverlap/scatter.%d.data",
      maxBucketSize);
  scatter = fopen(fileloc, "w");
  fprintf(scatter, "# bsize1 bsize2 numFirst numSecond level 0:1 1:1 0:0 sizeRatio overlap\n");
  sprintf(fileloc, "/home/francis/gnuplot/computeOverlap/overlapDiff.%d.data",
      maxBucketSize);
  fDiff = fopen(fileloc, "w");
  fprintf(fDiff, "# computed_overlap diff_av diff_sd diff_min diff_max\n");
  for (k = 0; k < NUM_LEVELS; k++) {
    sprintf(fileloc, "/home/francis/gnuplot/computeOverlap/%d.%d.data",
      k, maxBucketSize);
    foo[k] = fopen(fileloc, "w");
    fprintf(foo[k], "# numFirst numCommon numSamples overlap_av overlap_sd bsize1_sd level_sd overlap_min overlap_max\n");
  }

  for (i = 0; i < 100000; i++) {
    bsize1 = getRandInteger(2, maxBucketSize);
    sizeRatio = getRandFloat((float)1.0,(float)16.0);
    bsize2 = (int) ((float) bsize1 * sizeRatio);
    overlap = getRandInteger(1,100);

    bp1 = makeRandomBucket(bsize1);
    bp2 = makeRandomBucket(bsize2);
    absNumOverlap = (int) ((float) (bsize1 * overlap) / (float) 100.0);
    if (absNumOverlap > bsize1) {
      // should never happen
      badOverlap++;
      absNumOverlap = bsize1;
    }
    makeCompareBucketFixed(bp1, bp2, absNumOverlap);


    makeFilterFromBucket(bp1);
    makeFilterFromBucket(bp2);
    compareFullFilters(bp1, bp2, &c);

    if (c.level == 0) {  // error reporting
      numNoLevel++;
    }
    else if ((c.first >= MAX_VALUE) || (c.common >= MAX_VALUE)) {
      outOfBounds++;
    }
    else {
      //diffLevel = c.index1 - c.index2;
      diffLevel = 0;
      if ((diffLevel < 0) || (diffLevel >= NUM_LEVELS)) {
        printf("Error with diffLevel %d\n", diffLevel);
        exit(1);
      }
      if ((index = indices[diffLevel][c.first][c.common]) >= MAX_SAMPLES) {
        numFullEntries++;
      }
      else {
        values[diffLevel][c.first][c.common].samples[index] = overlap;
        bsize1Samples[diffLevel][c.first][c.common].samples[index] = bsize1;
        levelSamples[diffLevel][c.first][c.common].samples[index] = c.level;
        indices[diffLevel][c.first][c.common]++;
        computedOverlap = (int) ((float) c.overlap * (float) 1.5625);
        if (overlapIndex[computedOverlap] < 20) {
          overlapDiff[computedOverlap][overlapIndex[computedOverlap]] =
               (char) overlap - computedOverlap + 100;
          overlapIndex[computedOverlap]++;
        }
        if (index < 100) {
          skew = getRandFloat((float)-0.25, (float) 0.25);
          fprintf(scatter, "%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f, %.2f\n",
               (float) bsize1 + skew, (float) bsize2 + skew, 
               (float) c.first + skew, (float) c.second + skew, 
               (float) c.level + skew,
               (float) (c.second - c.common) + skew, (float) c.common + skew,
               (float) (1024 + c.common - c.first - c.second) + skew,
               ((float) bsize2 / (float) bsize1) + skew, 
               (float) overlap + skew, (float) c.overlap + skew,
               (float) (overlap - computedOverlap) + skew);
        }
      }
    }
    freeBucket(bp1);
    freeBucket(bp2);
  }

  for (i = 0; i < MAX_VALUE; i++) {
    for (j = 0; j < MAX_VALUE; j++) {
      for (k = 0; k < NUM_LEVELS; k++) {
        if (indices[k][i][j] > 0) {
          getStatsChar(&valuesS[k], 
               values[k][i][j].samples, indices[k][i][j]);
          getStatsChar(&bsize1S[k], 
               bsize1Samples[k][i][j].samples, indices[k][i][j]);
          getStatsChar(&levelS[k], 
               levelSamples[k][i][j].samples, indices[k][i][j]);
          fprintf(foo[k], "%d %d %d %.2f %.2f %.2f %.2f %.2f %.2f\n",
                 i, j, indices[k][i][j], valuesS[k].av, valuesS[k].sd,
                 bsize1S[k].sd, levelS[k].sd, valuesS[k].min, valuesS[k].max);
        }
      }
    }
  }
  for (i = 0; i < 101; i++) {
    if (overlapIndex[i] > 5) {
      getStatsChar(&diffS, &overlapDiff[i], overlapIndex[i]);
      fprintf(fDiff, "%d %.2f %.2f %.2f %.2f\n", i, diffS.av-100, diffS.sd, diffS.min-100, diffS.max-100);
    }
  }
  printf("%d full entries, %d no level, %d out of bounds, %d bad overlap compute\n", 
           numFullEntries, numNoLevel, outOfBounds, badOverlap);
  for (k = 0; i < NUM_LEVELS; k++) {
    fclose(foo[k]);
  }
}

#define DISCRETE_OVERLAP 1
#define CONTINUOUS_OVERLAP 2
testFilterInterpretation2(int numSamples, int overlapType, int maxBucketSize)
{
  bucket *bp1, *bp2;
  compare c;
  float *op, *oo, *os, *zo, *zz, *level;
  float skew, sizeRatio;
  mystats opS, ooS, zzS, zoS, osS, levelS;
  int bsize1, bsize2, i, j, overlap;
  FILE *fop, *foo, *fzo, *fzz;
  unsigned char fileloc[128];

  oo = (float *) calloc(numSamples, sizeof(float));
  op = (float *) calloc(numSamples, sizeof(float));
  os = (float *) calloc(numSamples, sizeof(float));
  zo = (float *) calloc(numSamples, sizeof(float));
  zz = (float *) calloc(numSamples, sizeof(float));
  level = (float *) calloc(numSamples, sizeof(float));

  sprintf(fileloc, "/home/francis/gnuplot/filterInterpret/oo-all.%d.%d.data", 
          numSamples, maxBucketSize);
  foo = fopen(fileloc, "w");
  fprintf(foo, "# bsize1, bsize2, overlap, sizeRatio, 1:*, 1:1, level \n");
  fprintf(foo, "# Overlap: 1 = none, 2 = 1/4, 3 = 1/2, 4 = complete\n");
  fprintf(foo, "# SizeRatio: 1 = same size (1/1), 10 = 1/10\n");
  sprintf(fileloc, "/home/francis/gnuplot/filterInterpret/op-all.%d.%d.data", 
          numSamples, maxBucketSize);
  fop = fopen(fileloc, "w");
  fprintf(fop, "# bsize1, bsize2, overlap, sizeRatio, 1+1, 1:1, level \n");
  fprintf(fop, "# Overlap: 1 = none, 2 = 1/4, 3 = 1/2, 4 = complete\n");
  fprintf(fop, "# SizeRatio: 1 = same size (1/1), 10 = 1/10\n");
  sprintf(fileloc, "/home/francis/gnuplot/filterInterpret/zo-all.%d.%d.data", 
          numSamples, maxBucketSize);
  fzo = fopen(fileloc, "w");
  fprintf(fzo, "# bsize1, bsize2, overlap, sizeRatio, 1:*, 0:1, level \n");
  fprintf(fzo, "# Overlap: 1 = none, 2 = 1/4, 3 = 1/2, 4 = complete\n");
  fprintf(fzo, "# SizeRatio: 1 = same size (1/1), 10 = 1/10\n");
  sprintf(fileloc, "/home/francis/gnuplot/filterInterpret/zz-all.%d.%d.data", 
          numSamples, maxBucketSize);
  fzz = fopen(fileloc, "w");
  fprintf(fzz, "# bsize1, bsize2, overlap, sizeRatio, 1:*, 0:0, level \n");
  fprintf(fzz, "# Overlap: 1 = none, 2 = 1/4, 3 = 1/2, 4 = complete\n");
  fprintf(fzz, "# SizeRatio: 1 = same size (1/1), 10 = 1/10\n");

  for (i = 0; i < 500000; i++) {
    bsize1 = getRandInteger(32, maxBucketSize);
    sizeRatio = getRandFloat((float)1.0,(float)16.0);
    bsize2 = (int) ((float) bsize1 * sizeRatio);
    if (overlapType == DISCRETE_OVERLAP) {
      overlap = getRandInteger(1,4);
    }
    else {
      overlap = getRandInteger(1,100);
    }

    for (j = 0; j < numSamples; j++) {
      bp1 = makeRandomBucket(bsize1);
      bp2 = makeRandomBucket(bsize2);

      if (overlapType == DISCRETE_OVERLAP) {
        switch (overlap) { 
          case 1:
            // do nothing, no overlap
            break;
          case 2:
            makeCompareBucketFixed(bp1, bp2, bsize1>>2);
            break;
          case 3:
            makeCompareBucketFixed(bp1, bp2, bsize1>>1);
            break;
          case 4:
            makeCompareBucketFixed(bp1, bp2, bsize1);  // complete overlap
            break;
        }
      } 
      else {
        makeCompareBucketFixed(bp1, bp2, 
                  (int) ((float) (bsize1 * overlap) / (float) 100.0));
      }

      makeFilterFromBucket(bp1);
      makeFilterFromBucket(bp2);
      compareFullFilters(bp1, bp2, &c);
      skew = (numSamples == 1)?getRandFloat((float)-0.25, (float) 0.25):0.0;
      oo[j] = (float) c.common + skew;
      op[j] = (float) c.first + (float) c.second + skew;
      os[j] = (float) c.first + skew;
      zo[j] = (float) (c.second - c.common) + skew;
      zz[j] = (float) (1024 + c.common - c.first - c.second) + skew;
      level[j] = c.level;  // no skew cause it won't be an axis
      if (c.level == 0) {  // error reporting
        printf("sizes %d and %d, levels %d and %d\n", bp1->bsize, bp2->bsize,
             bp1->filters[0].level, bp2->filters[0].level);
      }
      getStatsFloat(&ooS, oo, numSamples);
      getStatsFloat(&opS, op, numSamples);
      getStatsFloat(&osS, os, numSamples);
      getStatsFloat(&zoS, zo, numSamples);
      getStatsFloat(&zzS, zz, numSamples);
      getStatsFloat(&levelS, level, numSamples);
      freeBucket(bp1);
      freeBucket(bp2);
    }
    if ((numSamples == 1) && (ooS.av != ooS.min)) {
      printf("ERROR: av = %.2f, min = %.2f\n", ooS.av, ooS.min);
    }
    fprintf(foo, "%d %d %d %.2f %.2f %.2f %.2f\n", bsize1, bsize2, overlap, sizeRatio,
                osS.av, ooS.av, levelS.av);
    fprintf(fop, "%d %d %d %.2f %.2f %.2f %.2f\n", bsize1, bsize2, overlap, sizeRatio,
                opS.av, ooS.av, levelS.av);
    fprintf(fzo, "%d %d %d %.2f %.2f %.2f %.2f\n", bsize1, bsize2, overlap, sizeRatio,
                osS.av, zoS.av, levelS.av);
    fprintf(fzz, "%d %d %d %.2f %.2f %.2f %.2f\n", bsize1, bsize2, overlap, sizeRatio,
                osS.av, zzS.av, levelS.av);
  }
  fclose(foo);
  fclose(fop);
  fclose(fzo);
  fclose(fzz);
  free(oo); free(os); free(zo); free(zz);
}

#define NUM_SAMPLES 20
testFilterInterpretation1(unsigned char *filename, int shift)
{
  float oo[NUM_SAMPLES];	//  1:1
  float zz[NUM_SAMPLES];	//  0:0
  float zo[NUM_SAMPLES];	//  0:1
  float os[NUM_SAMPLES];	//  1:*
  mystats ooS, zzS, zoS, osS;
  bucket *bp1, *bp2;
  compare c;
  int bsize1, bsize2, i, overlap;
  FILE *fdata;
  unsigned char fileloc[128];

  for (overlap = 0; overlap < 3; overlap++) {
    sprintf(fileloc, "%s%s.%d.data", "/home/francis/gnuplot/filterInterpret/",
        filename, overlap);
    if ((fdata = fopen(fileloc, "w")) == NULL) {
      printf("fopen failed (%s, %s)\n", fileloc, filename); exit(1);
    }
    fprintf(fdata, "# Exclusive buckets\n");
    fprintf(fdata, "# bsize1, bsize2, 1:*, (2sd), 1:1, (2sd), 0:0, (2sd), 0:1, (2sd)\n");
    for (bsize1 = 32; bsize1 < 1025; bsize1 += 32) {
      for (i = 0; i < NUM_SAMPLES; i++) {
        bp1 = makeRandomBucket(bsize1);
        bsize2 = bsize1<<shift;
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, bsize1-(bsize1>>overlap));
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
      getStatsFloat(&ooS, oo, NUM_SAMPLES);
      getStatsFloat(&osS, os, NUM_SAMPLES);
      getStatsFloat(&zoS, zo, NUM_SAMPLES);
      getStatsFloat(&zzS, zz, NUM_SAMPLES);
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
        bsize1 = getRandInteger(32, 10000);
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
        bsize1 = getRandInteger(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        break;
      case HALF_OVERLAP_SAME:
        test = "Half Overlap, Same Bucket Size";
        bsize1 = getRandInteger(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>1));
        break;
      case QUARTER_OVERLAP_SAME:
        test = "Quarter Overlap, Same Bucket Size";
        bsize1 = getRandInteger(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>2));
        break;
      case EIGHTH_OVERLAP_SAME:
        test = "Quarter Overlap, Same Bucket Size";
        bsize1 = getRandInteger(32, 10000);
        bsize2 = bsize1;
        bp1 = makeRandomBucket(bsize1);
        bp2 = makeRandomBucket(bsize2);
        makeCompareBucketFixed(bp1, bp2, (bsize1>>3));
        break;
      case EXCLUSIVE_VARY:
        test = "Exclusive Match, Vary Bucket Size";
        // Assume enough randomness that collisions never happen
        bsize1 = getRandInteger(200, 5000);
        bsize2 = getRandInteger(200, 5000);
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
  getStatsFloat(&commonStats, common, MMAX);
  printStats("Common bits in filters", &commonStats, 1);
  getStatsFloat(&firstStats, first_exclusive, MMAX);
  printStats("Exclusive bits in first filter", &firstStats, 0);
  getStatsFloat(&secondStats, second_exclusive, MMAX);
  printStats("Exclusive bits in second filter", &secondStats, 0);
  getStatsFloat(&bsize1Stats, first_bsize, MMAX);
  printStats("Bucket Size First Filter", &bsize1Stats, 0);
  getStatsFloat(&bsize2Stats, second_bsize, MMAX);
  printStats("Bucket Size Second Filter", &bsize2Stats, 0);
  getStatsFloat(&levelStats, level, MMAX);
  printStats("Level Used", &levelStats, 0);
  fclose(fdata);
}

main()
{
  srand48((long int) 10);

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
  // testFilterInterpretation1("match", 0);
  // testFilterInterpretation1("half", 1);
  // testFilterInterpretation1("quarter", 2);
  // testFilterInterpretation2(1, DISCRETE_OVERLAP);
  // testFilterInterpretation2(20, DISCRETE_OVERLAP);
  //testFilterInterpretation2(1, CONTINUOUS_OVERLAP, 1000);
  //printf("done 1000\n");
  //testFilterInterpretation2(1, CONTINUOUS_OVERLAP, 5000);
  //printf("done 5000\n");
  //testFilterInterpretation2(1, CONTINUOUS_OVERLAP, 10000);
  //printf("done 10000\n");
  //computeOverlapValues(128);
  // computeOverlapValues(1000);
  // computeOverlapValues(500);
  //computeOverlapValues(5000);
  //computeOverlapValues(10000);
  // test_getNormal();
  // test_combineBuckets();
  // test_getNonOverlap();
  // test_sizesAreClose();
  // test_hsearch();
  // test_createHighTouchTable();
    test_highTouch();
}
