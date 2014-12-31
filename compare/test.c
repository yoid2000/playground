#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Verbose level 1 */
// #define V1

/* doDiffv1 passes a window of size `window` bits over the vectors
 * (highVal and lowVal).  It keeps the windows sync'd to likely
 * matching substrings.  It does this as follows:  when it finds a
 * matching substring, it increments both windows as starts a new
 * round.  Within a round, it looks for a matching substring by
 * shifting each window a distance of `spread` bits relative to the
 * other window.  Each time a match is found, the `numMatches` 
 * variable is incremented.  A perfect match produces (64-window+1)
 * numMatches. */
doDiffv1(int window, int spread, unsigned long highVal, unsigned long lowVal) {
  unsigned int i, j;
  unsigned long mask, high, low;
  int highShift, lowShift, foundMatch;
  unsigned int numMatches = 0;

  highShift = 64 - window;	/* init shift value if mask is 6 bits */
  lowShift = 64 - window;
  mask = 1;
  for (i = 1; i < window; i++) {
    mask <<= 1;
    mask |= 1;
  }
  while((highShift >= 0) || (lowShift >= 0)) {
#ifdef V1
    printf("....\n");
#endif
    foundMatch = 0;
    if (lowShift >= 0) {
      /* we let the shift window run negative, and just
       * forego the actual shift when it happens.  This
       * means that we replicate a few compares at the end */
      low = (lowVal >> lowShift) & mask;
    }
    /* first slide the high window and compare with low */
    for (i = 0; i < spread; i++) {
      if (highShift >= 0) {
        high = (highVal >> highShift) & mask;
      }
#ifdef V1
      printf("H(%d,%lx), L(%d,%lx)\n", highShift, high, lowShift, low);
#endif
      if (high == low) {
        if ((highShift >= 0) && (lowShift >= 0)) {
#ifdef V1
          printf("match!\n");
#endif
          numMatches++;
        }
        foundMatch = 1;
        break;
      }
      highShift--;
    }
    if (foundMatch == 0) {
      /* didn't find a match while shifting the highVal window,
       * so let's try shifting the lowVal window */
      /* put the highVal window back */
      highShift += spread;
      if (highShift >= 0) {
        high = (highVal >> highShift) & mask;
      }
      lowShift--;
      for (i = 0; i < (spread-1); i++) {
        if (lowShift >= 0) {
          low = (lowVal >> lowShift) & mask;
        }
#ifdef V1
        printf("H(%d,%lx), L(%d,%lx)\n", highShift, high, lowShift, low);
#endif
        if (high == low) {
          if ((highShift >= 0) && (lowShift >= 0)) {
#ifdef V1
            printf("match!\n");
#endif
            numMatches++;
          }
          foundMatch = 1;
          break;
        }
        lowShift--;
      }
    }
    if (foundMatch == 0) {
      /* move both shift's to the next increment */
      highShift--;
      lowShift += (spread-1);
    }
    else {
      /* since we found a match, we "synch" the two windows to
       * this match, on the assumption that one or the other
       * windows had an "extra" bit.  This corrects for that
       * extra bit.  This could be an incorrect assumption, but
       * we assume that the the spread is wide enough to
       * correct for it later */
      highShift--;
      lowShift--;
    }
  }
  return(numMatches);
}

doDiffv2(int window, int spread, unsigned long highVal, unsigned long lowVal) {
  int i, j;
  unsigned long mask, high, low;
  int temp;
  unsigned int numMatches = 0;

  mask = 1;
  for (i = 1; i < window; i++) {
    mask <<= 1;
    mask |= 1;
  }
#ifdef V1
  printf("highVal = %lx, lowVal = %lx\n", highVal, lowVal);
#endif

  high = highVal;
  for (i = (64-window); i >= 0; i--) {
    temp = 64 - window - i;
    low = lowVal >> temp;
    // the following setting of j prevents overrunning the shift at the end
    if (i < (spread-1)) { j = (spread - 1 - i); }
    else { j = 0; }
    for (; j < spread; j++) {
#ifdef V1
      printf("H(%d,%lx), L(%d,%lx)\n", i, high, j, low);
#endif
      if ((high&mask) == (low&mask)) {
        numMatches++;
#ifdef V1
          printf("match!\n");
#endif
      }
      low >>= 1;
    }
    high >>= 1;
  }
  low = lowVal;
  spread--;
  for (i = (64-window); i > 0; i--) {
    temp = 64 - window - i + 1;
    high = highVal >> temp;
    if (i < spread) { j = (spread - i); }
    else { j = 0; }
    for (; j < spread; j++) {
#ifdef V1
      printf("H(%d,%lx), L(%d,%lx)\n", i, high, j, low);
#endif
      if ((low&mask) == (high&mask)) {
        numMatches++;
#ifdef V1
          printf("match!\n");
#endif
      }
      high >>= 1;
    }
    low >>= 1;
  }
  return(numMatches);
}

doOneTest(int (*doDiff)(), int window, int spread, unsigned char* testNum, unsigned long v1, unsigned long v2, unsigned int expectedNumMatches) {
  unsigned int numMatches;

#ifdef V1
  printf("---- TEST %s ----\n", testNum);
#endif
  if ((numMatches = (*doDiff)(window, spread, v1, v2)) != expectedNumMatches) {
    printf("FAIL1 %s\nv1 = 0x%lx\nv2 = 0x%lx\nExpected %d, got %d\n", 
        testNum, v1, v2, expectedNumMatches, numMatches);
    exit(0);
  }
  if ((numMatches = (*doDiff)(window, spread, v2, v1)) != expectedNumMatches) {
    printf("FAIL2 %s\nv1 = 0x%lx\nv2 = 0x%lx\nExpected %d, got %d\n", 
        testNum, v1, v2, expectedNumMatches, numMatches);
    exit(0);
  }
}

runCorrectnessTests() {
  unsigned long v1, v2;

  v1 = 0xfedcba9876543210;
  v2 = 0xfedcba9876543210;
  doOneTest(doDiffv2, 6, 4, "T00v2", v1, v2, 61);

  v1 = 0x0123456789abcdef;
  v2 = 0x0123456789abcdef;
  doOneTest(doDiffv1, 6, 4, "T01v1", v1, v2, 59);
  doOneTest(doDiffv2, 6, 4, "T01v2", v1, v2, 61);

  v2 = 0x0123456689abcdef;
//              ^
  doOneTest(doDiffv1, 6, 4, "T02v1", v1, v2, 53);
  doOneTest(doDiffv2, 6, 4, "T02v2", v1, v2, 55);

  v2 = 0x0123456489abcdef;
//              ^
  doOneTest(doDiffv1, 6, 4, "T03v1", v1, v2, 52);
  doOneTest(doDiffv2, 6, 4, "T03v2", v1, v2, 54);

  /* happens to be one "random" match */
  v2 = 0xfedcba9876543210;
//       ^^^^^^^^^^^^^^^^
  doOneTest(doDiffv1, 6, 4, "T04v1", v1, v2, 1);
  doOneTest(doDiffv2, 6, 4, "T04v2", v1, v2, 2);

  /* last 4 bytes shifted right by one */
  v2 = 0x0123456789ab66f7;
//                   >>>>(1)
  doOneTest(doDiffv1, 6, 4, "T05v1", v1, v2, 53);
  doOneTest(doDiffv2, 6, 4, "T05v2", v1, v2, 55);

  /* whole thing shifted right by one (1's shifted in)
   * (we get a match in the 1st round, then good from 
   * there with windows off by one.) */
  v2 = 0x8091a2b3c4d5e6f7;
//       >>>>>>>>>>>>>>>>(1)
  doOneTest(doDiffv1, 6, 4, "T06v1", v1, v2, 58);
  doOneTest(doDiffv2, 6, 4, "T06v2", v1, v2, 60);

  /* whole thing shifted right by three (1's shifted in)
   * (we get a match in the 1st round, then good from 
   * there with windows off by three.  This terminates the
   * whole thing 2 rounds early.) */
  v2 = 0xe02468acf13579bd;
//       >>>>>>>>>>>>>>>>(3)
  doOneTest(doDiffv1, 6, 4, "T07v1", v1, v2, 56);
  doOneTest(doDiffv2, 6, 4, "T07v2", v1, v2, 59);

  /* whole thing shifted right by five (1's shifted in)
   * (shift to big for windows to find, however, get
   * three "lucky" matches) */
  v2 = 0xf8091a2b3c4d5e6f;
//       >>>>>>>>>>>>>>>>(5)
  doOneTest(doDiffv1, 6, 4, "T08v1", v1, v2, 3);
  doOneTest(doDiffv2, 6, 4, "T08v2", v1, v2, 3);

  /* first half shifted right by one, 2nd half shifted
   * back left by one. (miss a couple in the middle before
   * betting back in sync) */
  v2 = 0x8091a2b389abcdef;
//       >>>>>>>><<<<<<<<(1)
  doOneTest(doDiffv1, 6, 4, "T09v1", v1, v2, 56);
  doOneTest(doDiffv2, 6, 4, "T09v2", v1, v2, 58);

  /* whole thing shifted right by five, 2nd half shifted
   * back left by 5. (should successfully gets back on sync
   * in the middle) */
  v2 = 0xf8091a2b89abcdef;
//       >>>>>>>>>>>>>>>>(5)
  doOneTest(doDiffv1, 6, 4, "T10v1", v1, v2, 30);
  doOneTest(doDiffv2, 6, 4, "T10v2", v1, v2, 30);

  v1 = 0x0000000000000000;
  v2 = 0x0000000000000000;
  doOneTest(doDiffv2, 6, 4, "T11v2", v1, v2, 401);

  v1 = 0xffffffffffffffff;
  v2 = 0xffffffffffffffff;
  doOneTest(doDiffv2, 6, 4, "T12v2", v1, v2, 401);

  printf("All correctness tests passed!\n");
}

#define BILLION 1000000000L
#define NUM_RUNS 1000000L
runSpeedTests() {
  unsigned long diff, v1, v2;
  struct timespec start, end;
  unsigned long i;

  v1 = 0x0123456789abcdef;
  v2 = 0x0123456789abcdef;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    doDiffv1(6, 4, v1, v2);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Fast elapsed = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Fast per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Fast Throughput is %d diffs per second\n", (int) (BILLION / (diff / NUM_RUNS)));

  v2 = 0xfedcba9876543210;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    doDiffv1(6, 4, v1, v2);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Slow elapsed = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Slow per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Slow Throughput is %d diffs per second\n", (int) (BILLION / (diff / NUM_RUNS)));
}

int cmpfunc (const void * a, const void * b)
{
   return ( *(int*)a - *(int*)b );
}

/* Ignore duplicate random numbers (on rare occasion, may be duplicate
 * user in a given bucket */
makeInitBucket(unsigned long* bp, int bsize) {
  int i;

  for (i = 0; i < bsize; i++) {
    *bp = lrand48();
    bp++;
  }
}

/* The higher the prob, the more overlap between the two buckets.
 * This routine produces a random number of differences. */
int
makeCompareBucketRandom(unsigned long* bp1, unsigned long* bp2, int bsize, double prob) {
  int i;
  int numDiffs = 0;

  for (i = 0; i < bsize; i++) {
    if (drand48() <= prob) {
      *bp2 = *bp1;
    }
    else {
      numDiffs++;
    }
    bp1++;
    bp2++;
  }
  return(numDiffs);
}

/* This routine must be called on pre-sorted buckets.  The User IDs
 * in the bucket must be randomly distributed */
makeCompareBucketFixed(unsigned long* bp1, unsigned long* bp2, int bsize, int numDiffs) {
  int i, numSame;
  
  numSame = bsize - numDiffs;
  if (numSame < 0) { numSame = 0; }

  for (i = 0; i < numSame; i++) {
    *bp2 = *bp1;
    bp1++;
    bp2++;
  }
}

printBucket(unsigned long* bp, int bsize) {
  int i;

  for (i = 0; i < bsize; i++) {
    printf("%4d: %lx\n", i, *bp);
    bp++;
  }
}

printBucketPair(unsigned long* bp1, unsigned long* bp2, int bsize) {
  int i;

  for (i = 0; i < bsize; i++) {
    printf("%4d: %lx, %lx\n", i, *bp1, *bp2);
    bp1++;
    bp2++;
  }
}

makeVectors(unsigned long* v1, unsigned long* v2, 
            unsigned long* bp1, unsigned long* bp2, int bsize) {
  int i, shiftNum;
  unsigned long *bpp1, *bpp2;

  *v1 = 0L;
  *v2 = 0L;


  bpp1 = bp1;
  bpp2 = bp2;
  shiftNum = 63;
  i = 0;
  for (shiftNum = 63; shiftNum >=0; shiftNum--) {
    *v1 |= ((*bpp1 & 1L) << shiftNum);
    *v2 |= ((*bpp2 & 1L) << shiftNum);

    /* if the bucket is smaller than 64, we just loop back around */
    if (++i == bsize) {
      bpp1 = bp1;
      bpp2 = bp2;
    }
    else
    {
      bpp1++;
      bpp2++;
    }
  }
}

/* This callibration produces real buckets, with an overlap determined
 * by a random function (according to the probabilities in the
 * probList[]).  The real buckets are then coded into our 64-bit
 * vectors, and compared. */
runCallibrationTestRandom(int bucketSize, int window, int spread, int numSamples, int detail) {
  unsigned long* bucket1;
  unsigned long* bucket2;
  int i, j, numMatches, numBucketDiffs;
  unsigned long v1, v2;
  double probList[12] = {0.999, 0.998, 0.995, 0.99, 0.98, 0.95, 0.9, 0.8, 0.7, 0.6, 0.5, 0.25};
  double prob, percentOverlap, percentDiff;
  int hits[10], h;  

  bucket1 = (unsigned long*) calloc(bucketSize, sizeof(unsigned long));
  bucket2 = (unsigned long*) calloc(bucketSize, sizeof(unsigned long));

  for (j = 0; j < 12; j++) {
    for (h = 0; h < 10; hits[h++] = 0);
    for (i = 0; i < numSamples; i++) {
      makeInitBucket(bucket1, bucketSize);
      makeInitBucket(bucket2, bucketSize);
      numBucketDiffs = makeCompareBucketRandom(bucket1, bucket2, bucketSize, probList[j]);
      qsort(bucket1, bucketSize, sizeof(unsigned long), cmpfunc);
      qsort(bucket2, bucketSize, sizeof(unsigned long), cmpfunc);
      makeVectors(&v1, &v2, bucket1, bucket2, bucketSize);
      numMatches = doDiffv1(window, spread, v1, v2);
      h = (64-window+1) - numMatches;
      if (h < 0) {
        printf("ERROR: h < 0\n");
        exit(1);
      }
      for (; h < 10; hits[h++]++);
      percentOverlap = ((double)bucketSize - (double)numBucketDiffs) / (double)bucketSize;
      percentDiff = (double)numBucketDiffs / (double)bucketSize;
      if (detail) {printf("%d, %d, %d, %lx, %lx\n", j, numBucketDiffs, numMatches, v1, v2);}
      //printf("%d buckets differences, %d vector matches, %lx, %lx\n", numBucketDiffs, numMatches, v1, v2);
    }
    if (detail == 0) {
      printf("B%d, W%d, S%d, P%.3f: ", bucketSize, window, spread, probList[j]);
      for (h = 0; h < 10; h++) {
        printf("%d:%.2f, ", h, (float) hits[h] / (float) numSamples);
      }
      printf("\n");
    }
  }
  free(bucket1);
  free(bucket2);
  printf("--------------------------------\n");
}


/* This callibration test generates the buckets with an exact known
 * known number of differences between them.  If the buckets are of
 * size 64, then the resulting vectors also have an exact known number
 * of differences.  When `detail` is 0, this test records and prints:
 *   the fraction of vectors that had at least X numMatches,
 *   the average, max, and min matches for each numDiff,
 *   the fraction of bucket differences that contributed to each
 *       numMatch.  
 * When `detail` is 1, this test prints the numDiffs and
 * numMatchs for every pair of vectors.
 *
 * NOTE: this code only tested for bucketSize = 64.
 */
runCallibrationTestFixed(int (*doDiff)(), int bucketSize, int window, int spread, int numGroups, int numSamples, int detail) {
  unsigned long* bucket1;
  unsigned long* bucket2;
  int i, k, numDiffs, numMatches, haveVal;
  unsigned long v1, v2;
#define MAX_NUM_MATCH 402
  int array[64][MAX_NUM_MATCH+1]; // numDiffs by max realistic numMatch
  int tav;	       // total average
  int gav;             // group average
  int tavmax;      // average of all group maxes
  int tavmin;      // average of all group mins
  int tmax;        // total max
  int tmin;        // total min
  int gmax;        // per group max
  int gmin;        // per group min
  FILE *fcsv, *ftxt;
  char filecsv[64];
  char filetxt[64];

  if (doDiff == doDiffv1) {
    sprintf(filecsv, "v1b%dw%ds%dng%dns%d.csv", bucketSize, window, spread, numGroups, numSamples);
    sprintf(filetxt, "v1b%dw%ds%dng%dns%d.txt", bucketSize, window, spread, numGroups, numSamples);
  }
  else {
    sprintf(filecsv, "v2b%dw%ds%dng%dns%d.csv", bucketSize, window, spread, numGroups, numSamples);
    sprintf(filetxt, "v2b%dw%ds%dng%dns%d.txt", bucketSize, window, spread, numGroups, numSamples);
  }

  fcsv = fopen(filecsv, "w");
  ftxt = fopen(filetxt, "w");


  for (numMatches = MAX_NUM_MATCH; numMatches >= 0; numMatches--) {
    for (numDiffs = 0; numDiffs < 64; numDiffs++) {
      array[numDiffs][numMatches] = 0;
    }
  }

  bucket1 = (unsigned long*) calloc(bucketSize, sizeof(unsigned long));
  bucket2 = (unsigned long*) calloc(bucketSize, sizeof(unsigned long));

  if (detail == 2) {
    printf("numDiff, av , min, max, B%d, W%d, S%d, D%d\n ", bucketSize, window, spread, numDiffs);
  }
  for (numDiffs = 0; numDiffs < 64; numDiffs++) {
    tav = 0;
    tavmax = 0;
    tavmin = 0;
    tmax = 0;
    tmin = 10000;
    for (k = 0; k < numGroups; k++) {
      gav = 0;
      gmax = 0;
      gmin = 10000;
      for (i = 0; i < numSamples; i++) {
        makeInitBucket(bucket1, bucketSize);
        makeInitBucket(bucket2, bucketSize);
        makeCompareBucketFixed(bucket1, bucket2, bucketSize, numDiffs);
        qsort(bucket1, bucketSize, sizeof(unsigned long), cmpfunc);
        qsort(bucket2, bucketSize, sizeof(unsigned long), cmpfunc);
        makeVectors(&v1, &v2, bucket1, bucket2, bucketSize);
        numMatches = (*doDiff)(window, spread, v1, v2);
        if (detail == 2) {printf("%d, %d, %lx, %lx\n", numDiffs, numMatches, v1, v2);}
        if (numMatches < MAX_NUM_MATCH) {
          array[numDiffs][numMatches]++;
        }
        gav += numMatches;
        gmin = numMatches<gmin?numMatches:gmin;
        gmax = numMatches>gmax?numMatches:gmax;
        tav += numMatches;
        tmin = numMatches<tmin?numMatches:tmin;
        tmax = numMatches>tmax?numMatches:tmax;
      }
      if (detail == 2) {
        printf("%d, %.2f, %d, %d\n", numDiffs, (float)gav/(float)numSamples, gmin, gmax);
      }
      tavmin += gmin;
      tavmax += gmax;
    }
    fprintf(fcsv, "%d, %.2f, %.2f, %.2f, %d, %d\n", numDiffs, (float)tav/(float)(numGroups*numSamples), (float)tavmin/(float)numGroups, (float)tavmax/(float)numGroups, tmin, tmax);
  }
  for (numMatches = MAX_NUM_MATCH; numMatches >= 0; numMatches--) {
    haveVal = 0;
    for (numDiffs = 0; numDiffs < 64; numDiffs++) {
      if (array[numDiffs][numMatches] != 0) {
        haveVal = 1;
        break;
      }
    }
    if (haveVal) {
      fprintf(ftxt, "%d Matches: ", numMatches);
      for (numDiffs = 0; numDiffs < 64; numDiffs++) {
        if (array[numDiffs][numMatches] != 0) {
          fprintf(ftxt, "%d:%d, ", numDiffs, array[numDiffs][numMatches]);
        }
      }
      fprintf(ftxt, "\n");
    }
  }
  free(bucket1);
  free(bucket2);
  fclose(fcsv);
  fclose(ftxt);
  fprintf(fcsv, "--------------------------------\n");
}

main() 
{
  // runCorrectnessTests();
  // runSpeedTests();

  srand48((long int) 1);

  // runCallibrationTestRandom(1000, 6, 2, 100, 0);
  // runCallibrationTestRandom(1000, 6, 4, 100, 0);
  // runCallibrationTestRandom(1000, 8, 2, 100, 0);
  // runCallibrationTestRandom(1000, 8, 6, 100, 0);
  // runCallibrationTestRandom(1000, 50, 6, 100, 0);
  runCallibrationTestFixed(doDiffv1, 64, 5, 2, 100, 20, 0);
  runCallibrationTestFixed(doDiffv1, 64, 6, 2, 100, 20, 0);
  runCallibrationTestFixed(doDiffv1, 64, 6, 4, 100, 20, 0);
  runCallibrationTestFixed(doDiffv1, 64, 8, 2, 100, 20, 0);
  runCallibrationTestFixed(doDiffv1, 64, 8, 6, 100, 20, 0);
  runCallibrationTestFixed(doDiffv1, 64, 40, 6, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 6, 2, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 6, 4, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 6, 6, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 6, 8, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 8, 8, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 8, 10, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 4, 2, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 4, 4, 100, 20, 0);
  runCallibrationTestFixed(doDiffv2, 64, 4, 6, 100, 20, 0);
}
