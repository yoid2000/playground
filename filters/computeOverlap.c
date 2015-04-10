
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "./filters.h"

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *dupBucket(bucket *arg1);
extern float getRandFloat(float arg1, float arg2);

#define MAX_SAMPLES 16
typedef struct mysamples_t {
  unsigned char samples[MAX_SAMPLES]; 
} mysamples;

FILE *outfile;

#define MAX_VALUE 1024
#define NUM_LEVELS 1
// Put these here (global) cause won't go on stack...
mysamples values[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
mysamples bsize1Samples[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
mysamples levelSamples[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
unsigned char indices[NUM_LEVELS][MAX_VALUE][MAX_VALUE];
unsigned char overlapDiff[101][20];
unsigned char overlapIndex[101];

computeOverlapValues(int minBucket,
                     int maxBucket, 
                     int numSamples, 
                     int numSamplesExp, 
                     float sizeRatioMin, 
                     float sizeRatioMax, 
                     char *dir)
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

  sprintf(fileloc, "%s/%d.%d.%d.%.2f.%.2f.scatter.data", dir, 
            minBucket, maxBucket, numSamplesExp, sizeRatioMin, sizeRatioMax);
  if ((scatter = fopen(fileloc, "w")) == NULL) {
    printf("Failed to fopen file %s\n", fileloc);
    exit(1);
  }
  fprintf(scatter, "# bsize1 bsize2 numFirst numSecond level 0:1 1:1 0:0 sizeRatio trueOverlap computedOverlap overlapDiff\n");


  sprintf(fileloc, "%s/%d.%d.%d.%.2f.%.2f.overlapDiff.data", dir, 
           minBucket, maxBucket, numSamplesExp, sizeRatioMin, sizeRatioMax);
  if ((fDiff = fopen(fileloc, "w")) == NULL) {
    printf("Failed to fopen file %s\n", fileloc);
    exit(1);
  }
  fprintf(fDiff, "# computed_overlap diff_av diff_sd diff_min diff_max\n");

  for (k = 0; k < NUM_LEVELS; k++) {
    sprintf(fileloc, "%s/%d.%d.%d.%.2f.%.2f.%d.base.data", dir, 
          minBucket, maxBucket, numSamplesExp, sizeRatioMin, sizeRatioMax, k);
    if ((foo[k] = fopen(fileloc, "w")) == NULL) {
      printf("Failed to fopen file %s\n", fileloc);
      exit(1);
    }
    fprintf(foo[k], "# numFirst numCommon numSamples overlap_av overlap_sd bsize1_sd level_sd overlap_min overlap_max\n");
  }

  for (i = 0; i < numSamples; i++) {
    bsize1 = getRandInteger(minBucket, maxBucket);
    sizeRatio = getRandFloat(sizeRatioMin,sizeRatioMax);
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
  fclose(fDiff);
  fclose(scatter);
}

printCommandLines()
{
  printf("Usage: ./computeOverlap minBucket maxBucket numSamples sizeRatioMin sizeRatioMaxoutputDir\n");
  printf("      minBucket = Minimum size of the smaller bucket (> 1)\n");
  printf("      maxBucket = Maximum size of the smaller bucket (> minBucket)\n");
  printf("      numSamples = Exponent (10^^numSamples) for the number of bucket pairs produced\n");
  printf("      sizeRatioMin = Smallest possible size ratio (>= 1)\n");
  printf("      sizeRatioMax = Largest possible size ratio (<= 16)\n");
  printf("      outputDir = Directory where output files go\n");
}

main(int argc, char *argv[])
{
  int minBucket, maxBucket, numSamples, numSamplesExp;
  double sizeRatioMin, sizeRatioMax;
  char *dir;

  outfile = stdout;

  srand48((long int) 10);

  if (argc != 7) {
    printCommandLines();
    exit(1);
  }

  minBucket = atoi(argv[1]);
  if (minBucket < 1) {
    printCommandLines();
    exit(1);
  }

  maxBucket = atoi(argv[2]);
  if (maxBucket <= minBucket) {
    printCommandLines();
    exit(1);
  }

  numSamplesExp = atoi(argv[3]);
  numSamples = (int)pow((double)10, (double)numSamplesExp);
  if (numSamples < 10) {
    printf("numSamples rather small (10^%d = %d)\n", numSamplesExp, numSamples);
    printCommandLines();
    exit(1);
  }

  sizeRatioMin = atof(argv[4]);
  if (sizeRatioMin < 1) {
    printf("Bad sizeRatioMin %.2f\n", sizeRatioMin);
    printCommandLines();
    exit(1);
  }

  sizeRatioMax = atof(argv[5]);
  if (sizeRatioMax < 1) {
    printf("Bad sizeRatioMax %.2f\n", sizeRatioMax);
    printCommandLines();
    exit(1);
  }

  dir = argv[6];

  computeOverlapValues(minBucket, maxBucket, numSamples, numSamplesExp, (float)sizeRatioMin, (float)sizeRatioMax, dir);
}
