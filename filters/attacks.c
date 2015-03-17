#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#include "./filters.h"
#include "./utilities.h"
#include "./attacks.h"

extern bucket *makeBucket(int arg1);
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeRandomBucketFromList(int arg1, bucket *arg2);
extern bucket *dupBucket(bucket *arg1);
extern bucket *combineBuckets(bucket *arg1, bucket *arg2);
extern bucket *createHighTouchTable(int arg1);
extern bucket *makeSegregatedBucketFromList(int mask, 
                             int sampleNum,
                             int max_bsize,
                             unsigned int victim,
                             bucket *userList, 
                             int userListSize);
extern blocks *defineBlocks(int samples, int blocksPerSample, int *lastBlock);
extern float putBucket(bucket *bp, attack_setup *as);

#define USER_LIST_SIZE 0x100000  

float *diffAttackDiffs;
int attackRoundNum;
int rightGuesses;
int wrongGuesses;
int notSure;

float *accuracy;
int accIndex;
int accSize;

// This routine lets us avoid having too large, or too
// small bucket sizes (given that the number of needed
// segregate buckets can vary widely)
int
getSegregateMask(attack_setup *as, int *mask)
{
  int totalNumClusterBlocks;
  int totalNumBlocks;
  int blocksPerBucket;
  int blockSize;

  // the plus 1 is to make sure we compute 2 blocks for the zig-zag case
  totalNumClusterBlocks = worstCaseBlocks(as) + 1;

  blocksPerBucket = (int)((float)(totalNumClusterBlocks * 2) /
                          (float)(as->numLeftBuckets + as->numRightBuckets));

  blockSize = (int)((float)(as->usersPerBucket) / (float) blocksPerBucket);

  // now pick a mask that gives us something bigger than this block size
  *mask = 0xff; 
  while (*mask < (unsigned int) 0xffffff) {
    if ((int)((float) USER_LIST_SIZE / (float) *mask) < blockSize) {
      // this overshoots, so back off one shift
      *mask = *mask >> 1;
      break;
    }
    *mask = (*mask << 1) | 1;
  }
  if (*mask < (totalNumClusterBlocks * as->numSamples)) {
    printf("getSegregateMask() ERROR can't make enough unique blocks for the attack (%d, %d, %d)\n", *mask, totalNumBlocks, as->numSamples);
    exit(1);
  }
}

initAttackStats(int numSamples, attack_setup *as)
{
  int i;

  attackRoundNum = 0;
  for (i = 0; i < as->numRounds; i++) {
    diffAttackDiffs[i] = 0;
  }
  accSize = numSamples * as->numRounds * 
                                (as->maxLeftBuckets + as->maxRightBuckets);
  accuracy = (float *) calloc(accSize, sizeof (float *));
  accIndex = 0;
}

computeAttackStats(int numSamples, attack_setup *as) {
  mystats accS, diffS;

  getStatsFloat(&accS, accuracy, accIndex);
  getStatsFloat(&diffS, diffAttackDiffs, attackRoundNum);
  fprintf(as->f, "\n%d samples, right %d%%, answers: av %.2f, sd = %.2f, error: av = %.2f, sd = %.2f\n",
         numSamples,
         (int)(((float) rightGuesses / (float) as->numRounds) * 100.0),
         diffS.av, diffS.sd, accS.av, accS.sd);
  fprintf(as->f, "            answers: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         diffS.av, diffS.sd, diffS.min, diffS.max);
  fprintf(as->f, "            error: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         accS.av, accS.sd, accS.min, accS.max);
  
  free(accuracy);
}

makeChaffBuckets(bucket *userList, attack_setup *as) {
  int temp, bsize1, bsize2, i, chaffAmount;
  float noisyCount;
  int overlap, absNumOverlap;
  bucket *bp1, *bp2;
  compare c;

  chaffAmount = getRandInteger(as->chaffMin, as->chaffMax);
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
    noisyCount = putBucket(bp1, as);
    // printf("Attack: size %d, overlap %d\n", bp2->bsize, overlap);
    noisyCount = putBucket(bp2, as);
  }
}

int
worstCaseBlocks(attack_setup *as)
{
  return(as->numBaseBlocks + as->maxLeftBuckets + as->maxRightBuckets - 1);
}

int
makeClusterAndBuckets(mtm_cluster *mc, 
                      attack_setup *as, 
                      bucket *vbp,
                      bucket *userList,
                      blocks *block_array,
                      int baseBlocks, 
                      int blockIndex,
                      int numSamples, 
                      int perfect)
{
  int mask, max_bsize, shift, block;
  int j, s, b;
  int totalNumBlocks;
  bucket **bbp;   // temporary buckets for each block
  bucket *temp;

  getSegregateMask(as, &mask);

  if (perfect == 1) {
    definePerfectCluster(mc, as);
    totalNumBlocks = as->numLeftBuckets * as->numRightBuckets;
  }
  else {
    if (defineCluster(mc, baseBlocks, as) == 0) {
      printf("makeClusterAndBuckets ERROR %d\n", baseBlocks);
      printAttackSetup(as);
      printMtmCluster(mc);
      exit(1);
    }
    totalNumBlocks = worstCaseBlocks(as);
  }
  // build all the per-block user lists
  bbp = (bucket **) calloc(totalNumBlocks, sizeof(bucket *));
  for (j = 0; j < totalNumBlocks; j++) {
    bbp[j] = makeSegregatedBucketFromList(mask, blockIndex, mask * 3,
                       *(vbp->list), userList, USER_LIST_SIZE);
    blockIndex++;
    if ((bbp[j])->bsize < 1) {
      printf("Empty block\n");
      fprintf(as->f, "Empty block:\n");
      printAttackSetup(as);
    }
    if (blockIndex > mask) {
      printf("makeClusterAndBuckets() ERROR blockIndex (%d) too big (%d)\n",
                                                         blockIndex, mask);
      exit(1);
    }
  }

  // build the buckets from the per-block user lists
  for (s = 0; s < 2; s++) {
    for (b = 0; b < mc->numBuckets[s]; b++) {
      mc->bucket[s][b].bp = makeBucket(0);
      for (j = 0; j < mc->bucket[s][b].numBlocks; j++) {
        block = mc->bucket[s][b].blocks[j];
        temp = combineBuckets(bbp[block], mc->bucket[s][b].bp);
        freeBucket(mc->bucket[s][b].bp); mc->bucket[s][b].bp = temp;
      }
    }
  }

  // free all of the per-block user lists
  for (j = 0; j < totalNumBlocks; j++) {
    freeBucket(bbp[j]);
  }
  free(bbp);
  return(blockIndex);
}

float
oneAttack(int numSamples, 
             bucket *userList, 
             attack_setup *as)
{
  int i, j, blockIndex=0;
  int baseBlocks;
  float noisyCount;
  bucket *temp, *vbp;  // victim
  mtm_cluster mc;
  bucket *bp[2][MAX_NUM_BUCKETS_PER_SIDE];    // actual buckets
  int s, b;     // side and bucket for bp[][]
  blocks *block_array = NULL;
  int lastBlock, first_s;
  float sums[2];
  float av1, av2;

  sums[LEFT] = 0; sums[RIGHT] = 0;

  if ((as->maxLeftBuckets > MAX_NUM_BUCKETS_PER_SIDE) ||
      (as->maxRightBuckets > MAX_NUM_BUCKETS_PER_SIDE)) {
    printf("oneAttack ERROR1 %d, %d\n",  
                   as->maxLeftBuckets, as->maxRightBuckets);
    exit(1);
  }

  block_array = defineBlocks(numSamples, worstCaseBlocks(as), &lastBlock);

  // make victim
  vbp = makeRandomBucketFromList(1, userList);
  for (i = 0; i < numSamples; i++) {
    makeChaffBuckets(userList, as);
    as->numLeftBuckets = getRandInteger(as->minLeftBuckets, 
                                                  as->maxLeftBuckets);
    as->numRightBuckets = getRandInteger(as->minRightBuckets, 
                                                  as->maxRightBuckets);
    baseBlocks = as->numBaseBlocks + 
                 as->numLeftBuckets + as->numRightBuckets - 1;

    if ((blockIndex + baseBlocks) > lastBlock) {
      printf("oneAttack ERROR2 %d, %d, %d, %d\n", i, blockIndex, baseBlocks,
                                                 lastBlock);
      printAttackSetup(as);
      exit(1);
    }

    if (baseBlocks > MAX_NUM_BLOCKS) {
      printf("oneAttack ERROR3 %d\n",  baseBlocks);
      exit(1);
    }

    blockIndex = makeClusterAndBuckets(&mc, as, vbp, userList, block_array,
                                     baseBlocks, blockIndex, numSamples, 0);

    // ok, now we have a user-list bucket for each bucket in the
    // cluster.  We need to add the victim in the right place(s),
    // and set whether right or left will be submitted first
    if (as->attribute == VICTIM_ATTRIBUTE_YES) {
      if (as->location == VICTIM_IN_ONE_RIGHT) {
        if (as->order == VICTIM_FIRST) {
          first_s = RIGHT;
          b = 0;
        }
        else {
          first_s = LEFT;
          b = mc.numBuckets[RIGHT] - 1;
        }
        temp = combineBuckets(mc.bucket[RIGHT][b].bp, vbp);
        freeBucket(mc.bucket[RIGHT][b].bp); mc.bucket[RIGHT][b].bp = temp;
      }
      else {
        first_s = RIGHT;
        if (as->location == VICTIM_IN_ALL_RIGHT) { 
          s = RIGHT; 
          first_s = RIGHT;    // assumes order = VICTIM_FIRST
        }
        else if (as->location == VICTIM_IN_ALL_LEFT) { 
          s = LEFT; 
          first_s = LEFT;    // assumes order = VICTIM_FIRST
        }
        if (as->order == VICTIM_LAST) {
          // assumption wrong, so swap sides
          first_s = (first_s==RIGHT)?LEFT:RIGHT;
        }
        for (b = 0; b < mc.numBuckets[s]; b++) {
          temp = combineBuckets(mc.bucket[s][b].bp, vbp);
          freeBucket(mc.bucket[s][b].bp); mc.bucket[s][b].bp = temp;
        }
      }
    }
    // and finally feed them to the defense in the appropriate order.
    if (accIndex >= accSize) {
      printf("oneMtMAttack() accIndex %d exceeds %d\n", accIndex, accSize);
      exit(1);
    }
    s = first_s;
    for (b = 0; b < mc.numBuckets[s]; b++) {
      noisyCount = putBucket(mc.bucket[s][b].bp, as);
      accuracy[accIndex++] = (float)((mc.bucket[s][b].bp)->bsize) - noisyCount;
      sums[s] += noisyCount;
    }
    s = (s==RIGHT)?LEFT:RIGHT;
    for (b = 0; b < mc.numBuckets[s]; b++) {
      noisyCount = putBucket(mc.bucket[s][b].bp, as);
      accuracy[accIndex++] = (float)((mc.bucket[s][b].bp)->bsize) - noisyCount;
      sums[s] += noisyCount;
    }
  }
  // report
  free(block_array);
  freeBucket(vbp);
  av1 = (float)(sums[LEFT] / (float) numSamples);
  av2 = (float)(sums[RIGHT] / (float) numSamples);
  return(av2 - av1);
}

makeDecision(float answer, attack_setup *as)
{
  switch(as->attribute) {
    case VICTIM_ATTRIBUTE_YES:
      switch(as->location) {
        case VICTIM_IN_ALL_LEFT:
          if (answer <= -0.5) { rightGuesses++; }
          else { wrongGuesses++; }
          break;
        case VICTIM_IN_ALL_RIGHT:
        case VICTIM_IN_ONE_RIGHT:
          if (answer >= 0.5) { rightGuesses++; }
          else { wrongGuesses++; }
          break;
        default:
          printf("makeDecision() should not get here 6\n"); exit(1);
      }
      break;
    case VICTIM_ATTRIBUTE_NO:
      if ((answer < 0.5) && (answer > -0.5)) { rightGuesses++; }
      else { wrongGuesses++; }
      break;
    default:
      printf("makeDecision() should not get here 3\n"); exit(1);
  }
}

runAttack(bucket *userList, attack_setup *as)
{
  int confidence;   // between 0 and 100 percent
  int i, numSamples;
  float answer;
  
  initDefenseStats();
  initAttackStats(as->numSamples, as);
  rightGuesses = 0;
  notSure = 0;
  wrongGuesses = 0;
  for (i = 0; i < as->numRounds; i++) {
    initDefense(10000);
    answer = oneAttack(as->numSamples, userList, as);
    makeDecision(answer, as);
    endDefense();
    if (attackRoundNum == as->numRounds) {
      printf("runAttack(): bad attackRoundNum %d\n", attackRoundNum);
      exit(1);
    }
    diffAttackDiffs[attackRoundNum++] = answer;
  }
  computeAttackStats(as->numSamples, as);
  computeDefenseStats(as->numRounds, as);
}

printAttackSetup(attack_setup *as)
{
  if ((as->maxRightBuckets == as->minRightBuckets) &&
      (as->maxLeftBuckets == as->minLeftBuckets)) {
    fprintf(as->f, "%d:%d ", as->minLeftBuckets, as->minRightBuckets);
  }
  else {
    fprintf(as->f, "%d(%d):%d(%d) ", as->minLeftBuckets, as->maxLeftBuckets,
                                     as->minRightBuckets, as->maxRightBuckets);
  }

  fprintf(as->f, "Attack: %s, %s, %s, %s\n", 
                                     as->defense_str[as->defense], 
                                     as->order_str[as->order],
                                     as->location_str[as->location],
                                     as->attribute_str[as->attribute]);
  fprintf(as->f, " chaffMax %d\n", as->chaffMax);
  fprintf(as->f, " chaffMin %d\n", as->chaffMin);
  fprintf(as->f, " numRounds %d\n", as->numRounds);
  fprintf(as->f, " numSamples %d\n", as->numSamples);
  fprintf(as->f, " numBaseBlocks %d\n", as->numBaseBlocks);
}

printCommandLines(attack_setup *as)
{
  int i;

  printf("Usage: ./runAttacks -l min_left -r min_right -L max_left -R max_right -d defense -o victim_order -v victim_location -t victim_attribute -m min_chaff -x max_chaff -a num_rounds -s num_samples -e seed -B num_base_blocks_past_min -u user_per_bucket <directory>\n ");
  printf("     defense values:\n");
  for (i = 0; i < NUM_DEFENSES; i++) {
    printf("          %d = %s\n", i, as->defense_str[i]);
  }
  printf("     victim_order values:\n");
  for (i = 0; i < NUM_ORDERS; i++) {
    printf("          %d = %s\n", i, as->order_str[i]);
  }
  printf("     victim_location values:\n");
  for (i = 0; i < NUM_VICTIM_LOC; i++) {
    printf("          %d = %s\n", i, as->location_str[i]);
  }
  printf("     victim_attribute values:\n");
  for (i = 0; i < NUM_ATTRIBUTES; i++) {
    printf("          %d = %s\n", i, as->attribute_str[i]);
  }
  printf("     min_chaff and max_chaff >= 0\n");
  printf("     num_rounds (for experiment statistical significance) > 0\n");
  printf("     num_samples (for attack statistical significance) > 0\n");
  printf("     seed:  any integer\n");
  
  printf("     num_base_blocks_past_min >= 0\n");
  printf("     users_per_bucket > 0\n");
  printf("     min_left >= 1\n");
  printf("     max_left >= min_left (0 means same as min)\n");
  printf("     min_right >= 1\n");
  printf("     max_right >= min_right (0 means same as min)\n");
}

main(int argc, char *argv[])
{
  bucket *userList;
  attack_setup as;
  int ran, c, seed=0;
  unsigned char path[378], dir[128], filename[256], temp[32];

  filename[0] = '\0';

  as.defense_str[BASIC_DEFENSE] = "basic defense";
  as.defense_str[OtO_DEFENSE] = "OtO defense"; 
  as.defense_str[MtO_DEFENSE] = "MtO defense"; 
  as.defense_str[MtM_DEFENSE] = "MtM defense";
  as.order_str[VICTIM_FIRST] = "victim first"; 
  as.order_str[VICTIM_LAST] = "victim last"; 
  as.attribute_str[VICTIM_ATTRIBUTE_YES] = "victim has attribute"; 
  as.attribute_str[VICTIM_ATTRIBUTE_NO] = "victim does not have attribute"; 
  as.subAttack_str[RANDOM_CHILDREN] = "random children"; 
  as.subAttack_str[SEGREGATED_CHILDREN] = "segregated children"; 
  as.location_str[VICTIM_IN_PARENT] = "victim in parent";
  as.location_str[VICTIM_IN_ONE_CHILD] = "victim in one child";
  as.location_str[VICTIM_IN_ALL_CHILDREN] = "victim in all children";
  as.location_str[VICTIM_IN_ALL_LEFT] = "victim in all left";
  as.location_str[VICTIM_IN_ALL_RIGHT] = "victim in all right";
  as.location_str[VICTIM_IN_ONE_RIGHT] = "victim in one right";

  // set defaults
  as.defense = MtO_DEFENSE;
  as.order = VICTIM_LAST;
  as.location = VICTIM_IN_PARENT;
  as.attribute = VICTIM_ATTRIBUTE_YES;
  as.subAttack = SEGREGATED_CHILDREN;
  as.usersPerBucket = 200;
  as.chaffMin = 0;
  as.chaffMax = 0;
  as.numRounds = 20;
  as.numSamples = 10;
  as.numBaseBlocks = 0;
  as.minLeftBuckets = 2;   
  as.maxLeftBuckets = 0;   // set same as minLeftBuckets
  as.minRightBuckets = 2;   
  as.maxRightBuckets = 0;    // set same as minRightBuckets
  as.numLeftBuckets = 0;     // gets set later
  as.numRightBuckets = 0;     // gets set later

  while ((c = getopt (argc, argv, "u:l:L:r:R:B:a:d:o:v:t:c:m:x:a:s:e:h?")) != -1) {
    sprintf(temp, "%c%s", c, optarg);
    strcat(filename, temp);
    switch (c) {
      case 'u':
        as.usersPerBucket = atoi(optarg);
        break;
      case 'l':
        as.minLeftBuckets = atoi(optarg);
        break;
      case 'L':
        as.maxLeftBuckets = atoi(optarg);
        break;
      case 'r':
        as.minRightBuckets = atoi(optarg);
        break;
      case 'R':
        as.maxRightBuckets = atoi(optarg);
        break;
      case 'B':
        as.numBaseBlocks = atoi(optarg);
        break;
      case 'd':
        as.defense = atoi(optarg);
        break;
      case 'o':
        as.order = atoi(optarg);
        break;
      case 'v':
        as.location = atoi(optarg);
        break;
      case 't':
        as.attribute = atoi(optarg);
        break;
      case 'm':
        as.chaffMin = atoi(optarg);
        break;
      case 'x':
        as.chaffMax = atoi(optarg);
        break;
      case 'a':
        as.numRounds = atoi(optarg);
        break;
      case 's':
        as.numSamples = atoi(optarg);
        break;
      case 'e':
        seed = atoi(optarg);
        break;
      case '?':
      case 'h':
        printCommandLines(&as);
        exit(1);
      default:
        printCommandLines(&as);
        exit(1);
    }
  }
  if (seed == 0) {
    seed = time(NULL);
  }
  srand48((long int) seed);

  if (as.maxLeftBuckets == 0) {
    as.maxLeftBuckets = as.minLeftBuckets;
  }
  if (as.maxRightBuckets == 0) {
    as.maxRightBuckets = as.minRightBuckets;
  }

  ran = quick_hash(filename) & 0xfff;
  sprintf(temp, ".%d", ran);
  strcat(filename, temp);
  strcat(filename, ".txt");

  if (optind == argc) {
    strcpy(dir, "./");
  }
  else {
    strcpy(dir, argv[optind]);
  }

  sprintf(path, "%s%s", dir, filename);
  if ((as.f = fopen(path, "w")) == NULL) {
    printf("Couldn't open file %s\n", path);
    exit(1);
  }

  fesetround(1);   // sets rounding to nearest integer

  // Make complete list of users used for all attacks
  userList = createHighTouchTable(USER_LIST_SIZE);
  diffAttackDiffs = (float *) calloc(as.numRounds, sizeof(float *));

  printAttackSetup(&as);

  measureClusters(userList, &as); exit(1);
  //test_getSegregateMask(userList); exit(1);
  //runAttack(userList, &as);
  printf("Done: %s\n", filename);
}

/************  TESTS ****************/

#define WITHOUT_OVERLAP 0
#define WITH_OVERLAP 1
int
bucketsHaveACommonBlock(mtm_bucket *b1, mtm_bucket *b2)
{
  int i, j;
  for (i = 0; i < b1->numBlocks; i++) {
    for (j = 0; j < b2->numBlocks; j++) {
      if (b1->blocks[i] == b2->blocks[j]) { return(WITH_OVERLAP); }
    }
  }
  return(WITHOUT_OVERLAP);
}

printAllClusterBuckets(mtm_cluster *mc)
{
  int side, bucket;

  for (side = 0; side < 2; side++) {
    for (bucket = 0; bucket < mc->numBuckets[side]; bucket++) {
      printf("Side %d, Bucket %d:\n", side, bucket);
      printBucket(mc->bucket[side][bucket].bp);
    }
  }
}

// every user should appear exactly once per side
checkClusterCorrectness(mtm_cluster *mc, bucket *userList)
{
  bucket *bp;
  int side, bucket, index;
  unsigned int user;
  unsigned int *lp;
  int i, j;
  int numAppearances, numInList;
  // allow up to 3 duplicates, because for instance of sloppy hashing
  // or some overlap in randomness
  unsigned int dups[3];
  int numDups = 0;

  // check 1000 randomly selected users...
  for (i = 0; i < 1000; i++) {
    // first pick random user
    side = getRandInteger(0, 1);   // pick right or left side
    bucket = getRandInteger(0, mc->numBuckets[side] - 1);
    bp = mc->bucket[side][bucket].bp;
    index = getRandInteger(0, bp->bsize - 1);
    user = bp->list[index];
    // now exhaustively check to see that the user appears once per side
    for (side = 0; side < 2; side++) {
      numAppearances = 0;
      for (bucket = 0; bucket < mc->numBuckets[side]; bucket++) {
        bp = mc->bucket[side][bucket].bp;
        lp = bp->list;
        for (j = 0; j < bp->bsize; j++) {
          if (*lp == user) { numAppearances++; }
          *lp++;
        }
      }
      if (numAppearances != 1) {
        // check to see if this user is in the userList more than once
        numInList = 0;
        lp = userList->list;
        for (j = 0; j < userList->bsize; j++) {
          if (*lp == user) { numInList++; }
          *lp++;
        }
        if (numInList == 1) {
          dups[numDups++] = user;
          if (numDups == 3) {
            printf("checkClusterCorrectness() ERROR, users %x, %x, %x\n",
                                                  dups[0], dups[1], dups[2]);
            printMtmCluster(mc);
            printAllClusterBuckets(mc);
            printBucket(userList);
            exit(1);
          }
        }
        else {
          printf("numInList = %d\n", numInList);
        }
      }
    }
  }
}

#define NUM_M_TRIALS 100
#define MIN_B_PER_SIDE 2
#define MAX_B_PER_SIDE 16
#define NO_OVERLAP_VALUE 200	// not a valid value (> 100)
#define BUCKET_IS_CONNECTED 1
#define BUCKET_NOT_CONNECTED 0
#define BNUM(b,s) ((b<<1)+s)
int overlap_array[MAX_B_PER_SIDE][MAX_B_PER_SIDE];

/*
 *  Returns one if the second bucket is equal to or earlier than
 *  the first bucket in the cluster sequence.  The sequence order is
 *  defined as left side first (anything on left side comes before
 *  anything on right side).
 */
int
secondBucketIsBefore(int s1, int b1, int s2, int b2)
{
  if ((s2 == RIGHT) && (s1 == LEFT)) return(0);  // s2 is later
  if ((s2 == LEFT) && (s1 == RIGHT)) return(1);  // s2 is earlier
  // s2 and s1 on same side
  if (b2 > b1) return(0);  // s2 is later
  return(1);   // s2 is earlier or same
}

initOverlapArray()
{
  int i, j;

  for (i = 0; i < MAX_B_PER_SIDE; i++) {
    for (j = 0; j < MAX_B_PER_SIDE; j++) {
      overlap_array[i][j] = NO_OVERLAP_VALUE;
    }
  }
}

printOverlapArray(mtm_cluster *mc)
{
  int s1, s2, b1, b2;

  for (s1 = 0; s1 < 2; s1++) {
    for (b1 = 0; b1 < mc->numBuckets[s1]; b1++) {
      for (s2 = s1; s2 < 2; s2++) {
        for (b2 = 0; b2 < mc->numBuckets[s2]; b2++) {
          if (secondBucketIsBefore(s1, b1, s2, b2)) { continue; }
          printf("     %d %d: %d\n", BNUM(b1,s1), BNUM(b2,s2),
                       overlap_array[BNUM(b1,s1)][BNUM(b2,s2)]);
        }
      }
    }
  }
}

/*
 *  Returns the max overlap threshold at which the cluster is connected
 */
int
computeConnectedClusterThreshold(attack_setup *as, 
                                 mtm_cluster *mc)
{
  int connected[MAX_B_PER_SIDE*2];
  int i, j, threshold;
  int s1, s2, b1, b2;
  int connectionMade, completelyConnected;

  for (threshold = 95; threshold >= 0; threshold -= 5) {
    // initialize the connected array
    for (i = 0; i < MAX_B_PER_SIDE; i++) {
      connected[i] = BUCKET_NOT_CONNECTED;
    }
    // start from any bucket (side 0, bucket 0 in this case)
    connected[BNUM(0,0)] = BUCKET_IS_CONNECTED;
    // what follows won't be the most efficient way to find the
    // connected components, but it'll work
    while(1) {
      // walk through all buckets, and if already connected, add
      // the neighbors that are above threshold
      connectionMade = 0;
      for (s1 = 0; s1 < 2; s1++) {
        for (b1 = 0; b1 < mc->numBuckets[s1]; b1++) {
          if (connected[BNUM(b1,s1)] == BUCKET_IS_CONNECTED) {
            // ok, look at the link with the other buckets
            for (s2 = s1; s2 < 2; s2++) {
              for (b2 = 0; b2 < mc->numBuckets[s2]; b2++) {
                if (secondBucketIsBefore(s1, b1, s2, b2)) { continue; }
                if (connected[BNUM(b2,s2)] == BUCKET_IS_CONNECTED) {
                  // already connected, so don't need to check
                  continue;
                }
                if (overlap_array[BNUM(b1,s1)][BNUM(b2,s2)] >= threshold) {
                  // above threshold, so add to the connected array
                  connected[BNUM(b2,s2)] = BUCKET_IS_CONNECTED;
                  connectionMade = 1;
                }
              }
            }
          }
        }
      }
      if (connectionMade == 0) {
        // no progress made the last round, so we must be done
        break;
      }
    }
    // check to see if we are completely connected
    completelyConnected = 1;
    for (s1 = 0; s1 < 2; s1++) {
      for (b1 = 0; b1 < mc->numBuckets[s1]; b1++) {
        if (connected[BNUM(b1,s1)] == BUCKET_NOT_CONNECTED) {
          completelyConnected = 0;
        }
      }
    }
    if (completelyConnected) {
      // yay!  all done
      return(threshold);
    }
  }
  // should never get here
  printf("computeConnectedClusterThreshold() ERROR\n");
  exit(1);
}

measureClusters(bucket *userList, attack_setup *as)
{
  int numBuckets;
  int p, i, j, k, o, s1, s2, b1, b2;
  int mask, max_bsize, shift;
  int baseBlocks, lastBlock, total;
  blocks *block_array = NULL;
  bucket **bbp;   // temporary buckets for each block
  bucket *temp, *vbp;  // victim
  mtm_cluster mc;
  compare c;
  int overlap, size;
  int *vlow[2], *low[2], *high[2], *vhigh[2];
  int vlow_index[2], low_index[2], high_index[2], vhigh_index[2];
  mystats vlowS[2], lowS[2], highS[2], vhighS[2];
  int connectThreshold;

  fprintf(as->f, "perfect overlap left_buckets right_buckets bsize base_blocks vlow_frac vlow_av vlow_sd low_frac low_av low_sd high_frac high_av high_sd vhigh_frac vhigh_av vhigh_sd\n");
  fflush(as->f);

  // make max size arrays for holding data points
  i = MAX_B_PER_SIDE * MAX_B_PER_SIDE * NUM_M_TRIALS;
  for (o = 0; o < 2; o++) {
    vlow[o] = (int *) calloc(i, sizeof (int));
    low[o] = (int *) calloc(i, sizeof (int));
    high[o] = (int *) calloc(i, sizeof (int));
    vhigh[o] = (int *) calloc(i, sizeof (int));
  }

  // make victim
  vbp = makeRandomBucketFromList(1, userList);

  for (numBuckets = MIN_B_PER_SIDE; numBuckets <= MAX_B_PER_SIDE; numBuckets *= 2) {
    as->minLeftBuckets = numBuckets;
    as->maxLeftBuckets = numBuckets;
    as->minRightBuckets = numBuckets;
    as->maxRightBuckets = numBuckets;
    as->numLeftBuckets = numBuckets;
    as->numRightBuckets = numBuckets;
    for (i = 0; i <= 2; i++) {
      if (i == 0) {as->numBaseBlocks = 0;}
      else if (i == 1) {as->numBaseBlocks = numBuckets;}
      else if (i == 2) {
        as->numBaseBlocks = (int) pow((float) numBuckets, (float) i) -
                             as->numLeftBuckets - as->numRightBuckets + 1;
      }
      for (p = 0; p < 2; p++) {
        for (size = 50; size <= 400; size *= 2) {
          if (((float)(size * numBuckets) / (float)as->numBaseBlocks) < 5.0) {
            // would produce empty buckets...
            continue;
          }
          as->usersPerBucket = size;
          if ((p == 1) && i != 2) { continue; }
          // make perfect cluster if p == 1
          for (o = 0; o < 2; o++) {
            vlow_index[o]=0, low_index[o]=0, high_index[o]=0, vhigh_index[o]=0;
          }
          for (j = 0; j < NUM_M_TRIALS; j++) {
            initCluster(&mc);
            initOverlapArray();
            baseBlocks = as->numBaseBlocks + 
                       as->numLeftBuckets + as->numRightBuckets - 1;
            block_array = defineBlocks(1, worstCaseBlocks(as), &lastBlock);


            makeClusterAndBuckets(&mc, as, vbp, userList, block_array,
                                                      baseBlocks, 0, 1, p);

            // the following line of code was for testing
            // if (p == 1) { checkClusterCorrectness(&mc, userList); }

            // this shouldn't influence the measurements much, but go ahead
            // and add the victim to one arbitrary bucket
            temp = combineBuckets(mc.bucket[RIGHT][0].bp, vbp);
            freeBucket(mc.bucket[RIGHT][0].bp); mc.bucket[RIGHT][0].bp = temp;

            // make digests
            for (s1 = 0; s1 < 2; s1++) {
              for (b1 = 0; b1 < mc.numBuckets[s1]; b1++) {
                makeFilterFromBucket(mc.bucket[s1][b1].bp);
              }
            }

            // measure overlap between all pairs of buckets
            for (s1 = 0; s1 < 2; s1++) {
              for (b1 = 0; b1 < mc.numBuckets[s1]; b1++) {
                // check bucket size within specs
                if ((mc.bucket[s1][b1].bp)->bsize > (as->usersPerBucket * 3)) {
                  printf("measureClusters() ERROR bucket too big (%d of %d)\n",
                           (mc.bucket[s1][b1].bp)->bsize, as->usersPerBucket);
                  exit(1);
                              
                }
                if ((mc.bucket[s1][b1].bp)->bsize < 
                          (int)((float)(as->usersPerBucket) / (float)3.0)) {
                  printf("measureClusters() ERROR bucket too small (%d of %d)\n",
                           (mc.bucket[s1][b1].bp)->bsize, as->usersPerBucket);
                  printMtmCluster(&mc);
                  printAllClusterBuckets(&mc);
                  exit(1);
                              
                }
                for (s2 = s1; s2 < 2; s2++) {
                  for (b2 = 0; b2 < mc.numBuckets[s2]; b2++) {
                    if (secondBucketIsBefore(s1, b1, s2, b2)) { continue; }
                    o = bucketsHaveACommonBlock(&(mc.bucket[s1][b1]), 
                                                      &(mc.bucket[s2][b2]));
                    compareFullFilters(mc.bucket[s1][b1].bp, 
                                                  mc.bucket[s2][b2].bp, &c);
                    overlap = (int) ((float) c.overlap * (float) 1.5625);
                    overlap_array[BNUM(b1,s1)][BNUM(b2,s2)] = overlap;
                    if (overlap < 20) {
                      vlow[o][vlow_index[o]++] = overlap;
                    }
                    else if (overlap < 50) {
                      low[o][low_index[o]++] = overlap;
                    }
                    else if (overlap < 80) {
                      high[o][high_index[o]++] = overlap;
                    }
                    else {
                      vhigh[o][vhigh_index[o]++] = overlap;
                    }
                  }
                }
              }
            }
            // free all the buckets etc.
            for (s1 = 0; s1 < 2; s1++) {
              for (b1 = 0; b1 < mc.numBuckets[s1]; b1++) {
                freeBucket(mc.bucket[s1][b1].bp);
                mc.bucket[s1][b1].bp = NULL;  // shouldn't be necessary...
              }
            }
            free(block_array);
            connectThreshold = computeConnectedClusterThreshold(as, &mc);
            printOverlapArray(&mc);
            printMtmCluster(&mc);
            printf("%d %d %d %d %d %d\n", 
                       p, as->numLeftBuckets, as->numRightBuckets, 
                       as->usersPerBucket, as->numBaseBlocks,
                       connectThreshold);
            fflush(NULL);
          }
          //printMtmCluster(&mc);
          // report the results
          total = vlow_index[0]+low_index[0]+high_index[0]+vhigh_index[0]+
                  vlow_index[1]+low_index[1]+high_index[1]+vhigh_index[1];
          for (o = 0; o < 2; o++) {
            getStatsInt(&vlowS[o], vlow[o], vlow_index[o]);
            getStatsInt(&lowS[o], low[o], low_index[o]);
            getStatsInt(&highS[o], high[o], high_index[o]);
            getStatsInt(&vhighS[o], vhigh[o], vhigh_index[o]);
            fprintf(as->f, "%d %d %d %4d %3d %d    %.2f %.2f %.2f    %.2f %.2f %.2f     %.2f %.2f %.2f     %.2f %.2f %.2f\n",
                       p, o, as->numLeftBuckets, as->numRightBuckets, 
                       as->usersPerBucket, as->numBaseBlocks,
                       (float)((float)vlow_index[o]/(float)total), 
                       vlowS[o].av, vlowS[o].sd,
                       (float)((float)low_index[o]/(float)total), 
                       lowS[o].av, lowS[o].sd,
                       (float)((float)high_index[o]/(float)total), 
                       highS[o].av, highS[o].sd,
                       (float)((float)vhigh_index[o]/(float)total), 
                       vhighS[o].av, vhighS[o].sd);
            fflush(as->f);
          }
        }
      }
    }
  }
  freeBucket(vbp);
}

do_getSegregateMask(bucket *userList, attack_setup *as)
{
  int mask;
  bucket *bp;
  int baseBlocks;
  mtm_cluster mc;

  getSegregateMask(as, &mask);

  printf("\n%d samples, %d left, %d right, %d blocks, %d users, mask %x (%d)\n",
                     as->numSamples, as->numLeftBuckets, 
                     as->numRightBuckets, as->numBaseBlocks, 
                     as->usersPerBucket, mask, mask);
  printf("%d users per block\n", (int)((float)USER_LIST_SIZE/(float)mask));

  baseBlocks = as->numBaseBlocks + 
                 as->numLeftBuckets + as->numRightBuckets - 1;
  if (defineCluster(&mc, baseBlocks, as) == 0) {
    printf("ERROR defineCluster failed\n");
  }
  printMtmCluster(&mc);
}

test_getSegregateMask(bucket *userList)
{
  attack_setup as;

  as.numBaseBlocks = 0;
  as.usersPerBucket = 200;
  as.numLeftBuckets = 1;
  as.numSamples = 20;
  as.maxLeftBuckets = as.numLeftBuckets;
  as.numRightBuckets = as.numLeftBuckets;
  as.maxRightBuckets = as.numRightBuckets;

  do_getSegregateMask(userList, &as);
}
