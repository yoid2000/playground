
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "./filters.h"
#include "./overlap-values2.h"

extern float getRandFloat(float arg1, float arg2);

#define MAX_SAMPLES 100
typedef struct mysamples_t {
  unsigned char samples[MAX_SAMPLES]; 
} mysamples;

#define MAX_VALUE 1024
#define NUM_LEVELS 1
// [smaller][larger]
mysamples values[MAX_VALUE][MAX_VALUE];

int
setRandomBit(int *bits)
{
  int bit;
  int i = 0;

  while(1) {
    bit = getRandInteger(0, 1023);
    if (bits[bit] == 0) {
      bits[bit] = 1;
      return(bit);
    }
    if (++i >= 10000) { printf("setRandomBit ERROR\n"); exit(1); }
  }
}

computeOneOverlap(int x, int y, int o)
{
  int addBits, commonBits, bit;
  int xbits[1024], ybits[1024];
  int i, numx, numy, common;

  for (i = 0; i < 1024; i++) {
    xbits[i] = 0;
    ybits[i] = 0;
  }

  commonBits = (int) ((float) x * ((float) o / (float) 100.0));
  for (i = 0; i < commonBits; i++) {
    bit = setRandomBit(xbits);
    if (ybits[bit] != 0) { 
      printf("computeOneOverlap ERROR1 %d %d %d\n", x, y, o); 
      exit(1); 
    }
    ybits[bit] = 1;
  }
  addBits = y - commonBits;
  if (addBits < 0) { 
    printf("computeOneOverlap ERROR2 %d %d %d\n", x, y, o); 
    exit(1);
  }
  for (i = 0; i < addBits; i++) {
    bit = setRandomBit(ybits);
  }
  addBits = x - commonBits;
  if (addBits < 0) { 
    printf("computeOneOverlap ERROR3 %d %d %d\n", x, y, o); 
    exit(1);
  }
  for (i = 0; i < addBits; i++) {
    bit = setRandomBit(xbits);
  }
  numx = 0; numy = 0;
  common = 0;
  for (i = 0; i < 1024; i++) {
    if (ybits[i]) numy++;
    if (xbits[i]) numx++;
    if (xbits[i] && ybits[i]) common++;
  }
  if (numy != y) {
    printf("computeOneOverlap ERROR4 %d %d %d (%d)\n", x, y, o, numy); 
    exit(1);
  }
  if (numx != x) {
    printf("computeOneOverlap ERROR5 %d %d %d (%d)\n", x, y, o, numx); 
    exit(1);
  }
  return(common);
}

#define PRINT_ARRAYS_FOR_GNUPLOT
#ifdef PRINT_ARRAYS_FOR_GNUPLOT
main(int argc, char *argv[])
{
  FILE *f;
  int i, j;

  f = fopen("overlap0.data", "w");
  fprintf(f, "# num_ones_small num_ones_big overlap\n\n");
  for (i = 0; i < 1024; i++) {
    for (j = 0; j < 1024; j++) {
      fprintf(f, "%d %d %d\n", i, j, overlap_array_0[i][j]);
    }
  }
  fclose(f);

  f = fopen("overlap25.data", "w");
  fprintf(f, "# num_ones_small num_ones_big overlap\n\n");
  for (i = 0; i < 1024; i++) {
    for (j = 0; j < 1024; j++) {
      fprintf(f, "%d %d %d\n", i, j, overlap_array_25[i][j]);
    }
  }
  fclose(f);

  f = fopen("overlap50.data", "w");
  fprintf(f, "# num_ones_small num_ones_big overlap\n\n");
  for (i = 0; i < 1024; i++) {
    for (j = 0; j < 1024; j++) {
      fprintf(f, "%d %d %d\n", i, j, overlap_array_50[i][j]);
    }
  }
  fclose(f);

  f = fopen("overlap75.data", "w");
  fprintf(f, "# num_ones_small num_ones_big overlap\n\n");
  for (i = 0; i < 1024; i++) {
    for (j = 0; j < 1024; j++) {
      fprintf(f, "%d %d %d\n", i, j, overlap_array_75[i][j]);
    }
  }
  fclose(f);
}
#endif

#ifdef COMPUTE_COMPLETE_ARRAYS
main(int argc, char *argv[])
{
  int x, y, o;  // x and y are the number of 1 bits for the smaller and
                // larger buckets respectively.  o is the overlap value.
  int common, commonSum;
  int k;

  srand48((long int) 10);


  for (o = 0; o < 100; o += 25) {
    printf("unsigned short overlap_array_%d[1024][1024] = {\n", o);
    for (x = 1; x < 1024; x++) {
      printf("  {     // [%d][0]\n    ", x);
      for (y = 0; y < 1024; y++) {
        commonSum = 0;
        if (y >= x) {
          for (k = 0; k < 100; k++) {
            commonSum += computeOneOverlap(x, y, o);
          }
        }
        common = (int)((float)commonSum / (float) 100.0);
        printf("%d", common);
        if (y == 1023) {
          if (x == 1023) {
            printf("\n  }\n};\n\n");
          }
          else {
            printf("\n  },\n");
          }
        }
        else if ((y & 0xf) == 0xf) {
          printf(",\n    ");
        }
        else {
          printf(", ");
        }
      }
    }
  }
}
#endif

/*
 * The following computes data for a small number of points in the
 * space.
 */
#ifdef COMPUTE_A_FEW_FALUES
#define NUMVAL 5
main(int argc, char *argv[])
{
  int values[NUMVAL] = {0, 10, 50, 300, 600};
  int x, y, o;  // x and y are the number of 1 bits for the smaller and
                // larger buckets respectively.  o is the overlap value.
  int i, j, k;
  int common[100];
  mystats S;
  unsigned char str[500];
  FILE *f;

  srand48((long int) 10);


  for (i = 1; i < NUMVAL; i++) {
    x = values[i];
    for (j = 0; j < NUMVAL; j++) {
      y = x + values[j];
      sprintf(str, "ovl%d.%d.data", x, y);
      f = fopen(str, "w");
      fprintf(f, "x y overlap common_av common_min common_max common_sd+ common_sd-\n");
      if (y >= 1024) { break; }
      for (o = 0; o <= 100; o += 25) {
        initStats(&S);
        for (k = 0; k < 100; k++) {
          common[k] = computeOneOverlap(x, y, o);
        }
        getStatsInt(&S, common, 100);
        fprintf(f, "%d %d %d %.2f %.2f %.2f %.2f %.2f\n", x, y, o, 
                            S.av, S.min, S.max, S.av + S.sd, S.av - S.sd);
      }
      fclose(f);
    }
  }
}
#endif
