#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Verbose level 1 */
//#define V1

doDiff(unsigned long highVal, unsigned long lowVal) {
  unsigned int i, j;
  unsigned long mask, high, low;
  int highShift, lowShift, foundMatch;
  unsigned int numMatches = 0;

  highShift = 58;	/* init shift value if mask is 6 bits */
  lowShift = 58;
  mask = 0x3f;
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
    for (i = 0; i < 4; i++) {
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
      highShift += 4;
      if (highShift >= 0) {
        high = (highVal >> highShift) & mask;
      }
      lowShift--;
      for (i = 0; i < 3; i++) {
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
      lowShift += 3;
    }
    else {
      /* since we found a match, we "synch" the two windows to
       * this match, on the assumption that one or the other
       * windows had an "extra" bit.  This corrects for that
       * extra bit.  This could be an incorrect assumption, but
       * we assume that the the 4-bit slide is wide enough to
       * correct for it later */
      highShift--;
      lowShift--;
    }
  }
  return(numMatches);
}

doOneTest(unsigned char* testNum, unsigned long v1, unsigned long v2, unsigned int expectedNumMatches) {
  unsigned int numMatches;

#ifdef V1
  printf("---- TEST %s ----\n", testNum);
#endif
  if ((numMatches = doDiff(v1, v2)) != expectedNumMatches) {
    printf("FAIL1 %s\nv1 = 0x%lx\nv2 = 0x%lx\nExpected %d, got %d\n", 
        testNum, v1, v2, expectedNumMatches, numMatches);
    exit(0);
  }
  if ((numMatches = doDiff(v2, v1)) != expectedNumMatches) {
    printf("FAIL2 %s\nv1 = 0x%lx\nv2 = 0x%lx\nExpected %d, got %d\n", 
        testNum, v1, v2, expectedNumMatches, numMatches);
    exit(0);
  }
}

runCorrectnessTests() {
  unsigned long v1, v2;

  v1 = 0x0123456789abcdef;
  v2 = 0x0123456789abcdef;
  doOneTest("T01", v1, v2, 59);

  v2 = 0x0123456689abcdef;
//              ^
  doOneTest("T02", v1, v2, 53);

  v2 = 0x0123456489abcdef;
//              ^
  doOneTest("T03", v1, v2, 52);

  /* happens to be one "random" match */
  v2 = 0xfedcba9876543210;
//       ^^^^^^^^^^^^^^^^
  doOneTest("T04", v1, v2, 1);

  /* last 4 bytes shifted right by one */
  v2 = 0x0123456789ab66f7;
//                   >>>>(1)
  doOneTest("T05", v1, v2, 53);

  /* whole thing shifted right by one (1's shifted in)
   * (we get a match in the 1st round, then good from 
   * there with windows off by one.) */
  v2 = 0x8091a2b3c4d5e6f7;
//       >>>>>>>>>>>>>>>>(1)
  doOneTest("T06", v1, v2, 58);

  /* whole thing shifted right by three (1's shifted in)
   * (we get a match in the 1st round, then good from 
   * there with windows off by three.  This terminates the
   * whole thing 2 rounds early.) */
  v2 = 0xe02468acf13579bd;
//       >>>>>>>>>>>>>>>>(3)
  doOneTest("T07", v1, v2, 56);

  /* whole thing shifted right by five (1's shifted in)
   * (shift to big for windows to find, however, get
   * three "lucky" matches) */
  v2 = 0xf8091a2b3c4d5e6f;
//       >>>>>>>>>>>>>>>>(5)
  doOneTest("T08", v1, v2, 3);

  /* first half shifted right by one, 2nd half shifted
   * back left by one. (miss a couple in the middle before
   * betting back in sync) */
  v2 = 0x8091a2b389abcdef;
//       >>>>>>>><<<<<<<<(1)
  doOneTest("T09", v1, v2, 56);

  /* whole thing shifted right by five, 2nd half shifted
   * back left by 5. (should successfully gets back on sync
   * in the middle) */
  v2 = 0xf8091a2b89abcdef;
//       >>>>>>>>>>>>>>>>(5)
  doOneTest("T10", v1, v2, 30);

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
    doDiff(v1, v2);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Fast elapsed = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Fast per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Fast Throughput is %d diffs per second\n", (int) (BILLION / (diff / NUM_RUNS)));

  v2 = 0xfedcba9876543210;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < NUM_RUNS; i++) {
    doDiff(v1, v2);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  printf("Slow elapsed = %llu nanoseconds\n", (long long unsigned int) diff);
  printf("Slow per run = %d nanoseconds\n", (int) (diff / NUM_RUNS));
  printf("Slow Throughput is %d diffs per second\n", (int) (BILLION / (diff / NUM_RUNS)));
}

main() 
{
  runCorrectnessTests();
  runSpeedTests();
}
