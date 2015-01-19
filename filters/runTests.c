#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./utilities.h"
#include "./filters.h"

// externs need to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);
extern float getRandFloat(float arg1, float arg2);

#define MAX_SAMPLES 32
typedef struct mysamples_t {
  unsigned char samples[MAX_SAMPLES]; 
} mysamples;

// Put these here (global) cause won't go on stack...
mysamples values[512][512];
mysamples bsize1Samples[512][512];
mysamples levelSamples[512][512];
unsigned char indices[512][512];

computeOverlapValues(int maxBucketSize)
{
  bucket *bp1, *bp2;
  compare c;
  int bsize1, bsize2, i, j, overlap, index;
  int numFullEntries = 0;
  int numNoLevel = 0;
  int outOfBounds = 0;
  FILE *foo;
  unsigned char fileloc[128];
  float sizeRatio;
  mystats valuesS, bsize1S, levelS;


  for (i = 0; i < 512; i++) {
    for (j = 0; j < 512; j++) {
      indices[i][j] = 0;
    }
  }

  sprintf(fileloc, "/home/francis/gnuplot/computeOverlap/%d.data",
    maxBucketSize);
  foo = fopen(fileloc, "w");
  fprintf(foo, "# numFirst numCommon numSamples overlap_av overlap_sd bsize1_sd level_sd\n");

  for (i = 0; i < 10000000; i++) {
    bsize1 = getRandInteger(32, maxBucketSize);
    sizeRatio = getRandFloat((float)1.0,(float)16.0);
    bsize2 = (int) ((float) bsize1 * sizeRatio);
    overlap = getRandInteger(1,100);

    bp1 = makeRandomBucket(bsize1);
    bp2 = makeRandomBucket(bsize2);
    makeCompareBucketFixed(bp1, bp2, 
                (int) ((float) (bsize1 * overlap) / (float) 100.0));

    makeFilterFromBucket(bp1);
    makeFilterFromBucket(bp2);
    compareFullFilters(bp1, bp2, &c);

    if (c.level == 0) {  // error reporting
      numNoLevel++;
    }

    if ((c.first >= 512) || (c.common >= 512)) {
      outOfBounds++;
    }
    else if ((index = indices[c.first][c.common]) >= MAX_SAMPLES) {
      numFullEntries++;
    }
    else {
      values[c.first][c.common].samples[index] = overlap;
      bsize1Samples[c.first][c.common].samples[index] = bsize1;
      levelSamples[c.first][c.common].samples[index] = c.level;
      indices[c.first][c.common]++;
    }
    freeBucket(bp1);
    freeBucket(bp2);
  }

  for (i = 0; i < 512; i++) {
    for (j = 0; j < 512; j++) {
      if (indices[i][j] > 0) {
        getStatsChar(&valuesS, 
             values[i][j].samples, indices[i][j]);
        getStatsChar(&bsize1S, 
             bsize1Samples[i][j].samples, indices[i][j]);
        getStatsChar(&levelS, 
             levelSamples[i][j].samples, indices[i][j]);
        fprintf(foo, "%d %d %d %.2f %.2f %.2f %.2f\n",
               i, j, indices[i][j], valuesS.av, valuesS.sd,
               bsize1S.sd, levelS.sd);
      }
    }
  }
    printf("%d full entries, %d no level, %d out of bounds\n", 
           numFullEntries, numNoLevel, outOfBounds);
  fclose(foo);
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
  computeOverlapValues(1000);
  //computeOverlapValues(5000);
  computeOverlapValues(10000);
}
