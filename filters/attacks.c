#include "./filters.h"
#include "./utilities.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeRandomBucketFromList(int arg1, bucket *arg2);
extern bucket *dupBucket(bucket *arg1);
extern bucket *combineBuckets(bucket *arg1, bucket *arg2);
extern bucket *createHighTouchTable(int arg1);

#define USER_LIST_SIZE 16384    // gives 256 groups of 64 users, for instance

#define VICTIM_HAS_ATTRIBUTE 1
#define VICTIM_NO_ATTRIBUTE 2
#define NOT_SURE_ABOUT_VICTIM 3

// number of attack rounds needed to be confident in my confidence level
#define NUM_ROUNDS 40	

#define GUESS_RIGHT 0
#define GUESS_WRONG 1

// global cause I'm too lazy to pass these around  :(
int *accuracy;
int accIndex;

makeChaffBuckets(bucket *userList, int low, int high, int defend) {
  int temp, bsize1, bsize2, i, chaffAmount, noisyCount;
  int overlap, absNumOverlap;
  bucket *bp1, *bp2;
  compare c;

  chaffAmount = getRandInteger(low, high);
  for (i = 0; i < chaffAmount; i += 2) {
    bsize1 = getRandInteger(25, 400);
    bsize2 = getRandInteger(25, 400);
    if (bsize1 > bsize2) {
      temp = bsize2;
      bsize2 = bsize1;
      bsize1 = temp;
    }
    bp1 = makeRandomBucketFromList(bsize1, userList);
    bp2 = makeRandomBucketFromList(bsize2, userList);
    if (bsize1 & 1) {
      overlap = getRandInteger(0,/* zzzz 100*/2);
      absNumOverlap = (int) ((float) (bsize1 * overlap) / (float) 100.0);
      if (absNumOverlap > bsize1) {
      // should never happen
        absNumOverlap = bsize1;
      }
      //printf("Attack absNumOverlap %d (of %d)\n", absNumOverlap, bsize1);
      makeCompareBucketFixed(bp1, bp2, absNumOverlap);
    }
    else {
      overlap = 0;   // only for reporting
    }
    // printf("Attack: size %d, overlap %d\n", bp1->bsize, overlap);
    noisyCount = putBucket(bp1, defend);
    // printf("Attack: size %d, overlap %d\n", bp2->bsize, overlap);
    noisyCount = putBucket(bp2, defend);
  }
}

#define VICTIM_FIRST 1
#define VICTIM_LAST 2

int
oneNaiveDiffAttack(int numSamples, bucket *userList, int defend, int order)
{
  int i;
  int sum1=0, sum2=0;
  int noisyCount;
  float av1, av2;
  bucket *bp1, *bp2, *vbp, *temp;  // *vbp = victim

  // In the naive diff attack, one set of buckets will have the victim
  // if the victim has the attribute, and the other set of buckets will
  // have the victim otherwise.  Therefore for the sake of this simulation,
  // it doesn't matter which set we put the victim in.
  vbp = makeRandomBucketFromList(1, userList);
  //printf("********VICTIM is %x********\n", vbp->list[0]);
  makeChaffBuckets(userList, 25, 75, defend);
  for (i = 0; i < numSamples; i++){
    // There is a chance the victim will be in this bucket too
    // Statistically this won't effect things much
    bp1 = makeRandomBucketFromList(getRandInteger(20, 200), userList);
    bp2 = combineBuckets(bp1, vbp);
    // bp1 and bp2 are identical, except bp2 has victim
    if (order == VICTIM_FIRST) {
      // make bp1 hold the victim
      temp = bp1; bp1 = bp2; bp2 = temp;
    }
    noisyCount = putBucket(bp1, defend);
    accuracy[accIndex++] = bp1->bsize - noisyCount;
    sum1 += noisyCount;
    makeChaffBuckets(userList, 15, 30, defend);
    noisyCount = putBucket(bp2, defend);
    accuracy[accIndex++] = bp2->bsize - noisyCount;
    sum2 += noisyCount;
    makeChaffBuckets(userList, 15, 30, defend);
  }
  freeBucket(vbp);
  av1 = (float)((float) sum1 / (float) numSamples);
  av2 = (float)((float) sum2 / (float) numSamples);
  if ((av2 - av1) > 0.5) {
    return(VICTIM_HAS_ATTRIBUTE);
  }
  else {
    return(NOT_SURE_ABOUT_VICTIM);
  }
}

runNaiveDiffAttack(bucket *userList, int defend, int order)
{
  int confidence;   // between 0 and 100 percent
  int i, numSamples, answer;
  int rightGuesses;
  int accSize;
  mystats S;
  
  for (numSamples = 5; numSamples < 81; numSamples *= 2) {
    initDefenseStats();
    accSize = numSamples * NUM_ROUNDS * 2;
    accuracy = (int *) calloc(accSize, sizeof (int *));
    accIndex = 0;
    rightGuesses = 0;
    for (i = 0; i < NUM_ROUNDS; i++) {
      initDefense(10000);
      answer = oneNaiveDiffAttack(numSamples, userList, defend, order);
      endDefense();
      if (answer == VICTIM_HAS_ATTRIBUTE) {
        rightGuesses++;
      }
    }
    if (accIndex != accSize) {
      printf("Something wrong with accIndex %d (should be %d)\n", 
              accIndex, accSize);
      exit(1);
    }
    getStatsInt(&S, accuracy, accIndex);
    printf("%d samples, confidence %d%%, error: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
           numSamples,
           (int)(((float) rightGuesses / (float) NUM_ROUNDS) * 100.0),
           S.av, S.sd, S.min, S.max);
    free(accuracy);
    computeDefenseStats(NUM_ROUNDS);
  }
}

main()
{
  bucket *userList;

  srand48((long int) 1);

  // Make complete list of users used for all attacks
  userList = createHighTouchTable(USER_LIST_SIZE);

  //printf("Naive Diff Attack, no defense:\n");
  //runNaiveDiffAttack(userList, 0);
  printf("Naive Diff Attack, with defense:\n");
  runNaiveDiffAttack(userList, 1, VICTIM_LAST);
}
