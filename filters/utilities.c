#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "./utilities.h"
#include "./filters.h"
#include "./normalDist.h"

int
getRandInteger(int min_size, int max_size)
{
  int ret;

  float frac = (float) (max_size+1-min_size) / (float) 0x7fffffff;

  ret = (min_size + (int) ((float) lrand48() * frac));
  if (ret < min_size) {
    //printf("ERROR: bad getRandInteger, %d less than min %d\n", ret, min_size);
    ret = min_size;
  }
  if (ret > max_size) {
    //printf("ERROR: bad getRandInteger, %d greater than max %d\n", ret, max_size);
    ret = max_size;
  }
  return(ret);
}

float
getRandFloat(float min, float max)
{
  float ran;
  ran = (float)drand48();
  ran *= (max - min);
  ran += min;
  return(ran);
}

float
getNormal(float mean, float sd) {
  int index;
  float ran;

  index = getRandInteger(0, 9999);
  ran = normalDist[index];
  ran *= sd;
  ran += mean;
  return(ran);
}

/*
 *  Returns 1 if threshold exceeded, 0 otherwise
 *
 *  Threshold is considered exceeded if the value + noise
 *  is greater than the hardThresh, where noise is generated
 *  according to the standard deviation sd.
 */
int
exceedsNoisyThreshold(float hardThresh, float value, float sd)
{
  float noise;

  noise = getNormal(0.0, sd);
//printf("%.2f\n", noise);
  if ((value + noise) > hardThresh) { return(1); }
  else { return(0); }
}


float
getFixedNormal(uint64_t fix, float mean, float sd) {
  float ran;

  fix &= 0x1fff;
  ran = normalDist[fix];
  ran *= sd;
  ran += mean;
  return(ran);
}

printStats(unsigned char *str, mystats *s, int header)
{
  if (header) {
    printf("\n| Total | Av | SD | Min | Max | Desc |\n");
    printf("| --- | --- | --- | --- | --- | --- |\n");
  }
  printf("| %d | %.2f | %.2f | %.2f | %.2f | %s |\n",
      s->total, s->av, s->sd, s->min, s->max, str);
}

getStatsFloat(mystats *s, float *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = 100000000000.0;
  s->max = 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow((x[i] - s->av), 2);
    s->min = (x[i] < s->min)?x[i]:s->min;
    s->max = (x[i] > s->max)?x[i]:s->max;
  }
  s->var = sum1 / (float)n;
  s->sd = sqrt(s->var);
}

getStatsChar(mystats *s, unsigned char *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = (float) 100000000000.0;
  s->max = (float) 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + (float) x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow(((float)x[i] - s->av), 2);
    s->min = ((float)x[i] < s->min)?(float)x[i]:s->min;
    s->max = ((float)x[i] > s->max)?(float)x[i]:s->max;
  }
  s->var = sum1 / (float)n;
  s->sd = sqrt(s->var);
}

// This is the exact same as getStatsChar, except for the
// incoming array (must be a cleaner way to code this....)
getStatsInt(mystats *s, int *x, int n)
{
  int  i;
  float sum = 0, sum1 = 0;

  s->total = n;
  s->min = (float) 100000000000.0;
  s->max = (float) 0.0;
  for (i = 0; i < n; i++)
  {
    sum = sum + (float) x[i];
  }
  s->av = sum / (float)n;
  /* Compute variance and standard deviation */
  for (i = 0; i < n; i++)
  {
    sum1 = sum1 + pow(((float)x[i] - s->av), 2);
    s->min = ((float)x[i] < s->min)?(float)x[i]:s->min;
    s->max = ((float)x[i] > s->max)?(float)x[i]:s->max;
  }
  s->var = sum1 / (float)n;
  s->sd = sqrt(s->var);
}

uint64_t
myRand64()
{
  return((lrand48()<<48) ^ (lrand48()<<16) ^ lrand48());
}

countHighAndLowBits(bucket *bp)
{
  int i;
  int high = 0;
  int low = 0;
  uint64_t *lp;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    if (*lp & 0x8000000000000000) {high++;}
    if (*lp & 0x01) {low++;}
    lp++;
  }
  printf("%d high and %d low of %d entries\n\n", high, low, bp->bsize);
}


/*********** TESTS **********/

test_getNormal()
{
  float *samples;
  int i;
  mystats S;
  float mean=1.0, sd=5.0;

  samples = (float *) calloc(10000, sizeof(float));

  getStatsFloat(&S, normalDist, 10000);
  printf("normalDist[] has mean %.2f and sd %.2f\n",
           S.av, S.sd);

  for (i = 0; i < 10000; i++) {
    samples[i] = getNormal(mean, sd);
  }
  getStatsFloat(&S, samples, 10000);
  printf("getNormal for mean %.2f and sd %.2f: mean %.2f, sd %.2f\n",
           mean, sd, S.av, S.sd);
}
