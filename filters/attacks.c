#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fenv.h>
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
                             int sampleShift,
                             int max_bsize, 
                             int sampleNum,
                             int childNum,
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
getSegregateMask(int numSamples, 
                 int numChildren, 
                 int *mask, 
                 int *max_bsize, 
                 int *shift)
{
  int bsize;

  // first compute shift value needed to shift numSamples past
  // numChildren
  *shift = 1;
  *mask = 1;
  while (*mask < (unsigned int) 0xffff) {
    if (*mask > numChildren) {
      break;
    }
    *mask = (*mask << 1) | 1;
    (*shift)++;
  }

  bsize = (int) ((float)USER_LIST_SIZE / 
                               ((float)numSamples * (float)numChildren));
  if (bsize > 200) {
    bsize = 200;  // don't really need buckets bigger than this for testing
  }
  else if (bsize < 32) {
    printf("getSegregateMask bsize (%d) too small (%d samples, %d children)\n",
                         bsize, numSamples, numChildren);
    exit(1);
  }
  // now pick a mask that roughly gives us this bucket size
  *mask = 0xff; 
  while (*mask < (unsigned int) 0xffffff) {
    if ((int)((float) USER_LIST_SIZE / (float) *mask) < bsize) {
      break;
    }
    *mask = (*mask << 1) | 1;
  }
  // finally, set a conservative value for max expected bucket size
  *max_bsize = bsize * 3;
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
                      int numSamples)
{
  int mask, max_bsize, shift, block;
  int j, s, b;
  bucket **bbp;   // temporary buckets for each block
  bucket *temp;

  bbp = (bucket **) calloc(worstCaseBlocks(as), sizeof(bucket *));
  getSegregateMask(numSamples, worstCaseBlocks(as),
                                           &mask, &max_bsize, &shift);

  if (defineCluster(mc, baseBlocks, as) == 0) {
    printf("makeClusterAndBuckets ERROR %d\n", baseBlocks);
    printAttackSetup(as);
    printMtmCluster(mc);
    exit(1);
  }
  // build all the per-block user lists
  for (j = 0; j < worstCaseBlocks(as); j++) {
    bbp[j] = makeSegregatedBucketFromList(mask, shift, max_bsize, 
                       block_array[blockIndex].sampleNum, 
                       block_array[blockIndex].childNum, 
                       *(vbp->list), userList, USER_LIST_SIZE);
    blockIndex++;
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
  for (j = 0; j < worstCaseBlocks(as); j++) {
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
                                     baseBlocks, blockIndex, numSamples);

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

  printf("Usage: ./runAttacks -l min_left -r min_right -L max_left -R max_right -d defense -o victim_order -v victim_location -t victim_attribute -m min_chaff -x max_chaff -a num_rounds -s num_samples -e seed -B num_base_blocks_past_min -E num_extra_blocks <directory>\n ");
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
  printf("     num_extra_blocks >= 0\n");
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

  while ((c = getopt (argc, argv, "l:L:r:R:B:a:d:o:v:t:c:m:x:a:s:e:h?")) != -1) {
    sprintf(temp, "%c%s", c, optarg);
    strcat(filename, temp);
    switch (c) {
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

#define NUM_M_TRIALS 100
#define MIN_B_PER_SIDE 2
#define MAX_B_PER_SIDE 4
measureClusters(bucket *userList, attack_setup *as)
{
  int numBuckets;
  int i, j, k, o, s1, s2, b1, b2;
  int mask, max_bsize, shift;
  int baseBlocks, lastBlock, total;
  blocks *block_array = NULL;
  bucket **bbp;   // temporary buckets for each block
  bucket *temp, *vbp;  // victim
  mtm_cluster mc;
  compare c;
  int overlap;
  int *vlow[2], *low[2], *high[2], *vhigh[2];
  int vlow_index[2], low_index[2], high_index[2], vhigh_index[2];
  mystats vlowS[2], lowS[2], highS[2], vhighS[2];

  i = 0;
  fprintf(as->f, "%d:overlap %d:left_buckets %d:right_buckets %d:base_blocks %d:extra_blocks %d:vlow_frac %d:vlow_av %d:vlow_sd %d:low_frac %d:low_av %d:low_sd %d:high_frac %d:high_av %d:high_sd %d:vhigh_frac %d:vhigh_av %d:vhigh_sd\n", i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i++, i);
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

  for (numBuckets = MIN_B_PER_SIDE; numBuckets <= MAX_B_PER_SIDE; numBuckets++) {
    as->minLeftBuckets = numBuckets;
    as->maxLeftBuckets = numBuckets;
    as->minRightBuckets = numBuckets;
    as->maxRightBuckets = numBuckets;
    as->numLeftBuckets = numBuckets;
    as->numRightBuckets = numBuckets;
    for (i = 0; i < 5; i += 2) {
      as->numBaseBlocks = i;
      for (o = 0; o < 2; o++) {
        vlow_index[o]=0, low_index[o]=0, high_index[o]=0, vhigh_index[o]=0;
      }
      for (j = 0; j < NUM_M_TRIALS; j++) {
        initCluster(&mc);
        baseBlocks = as->numBaseBlocks + 
                   as->numLeftBuckets + as->numRightBuckets - 1;
        block_array = defineBlocks(1, worstCaseBlocks(as), &lastBlock);


        makeClusterAndBuckets(&mc, as, vbp, userList, block_array,
                                            baseBlocks, 0, 1);

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
            for (s2 = s1; s2 < 2; s2++) {
              for (b2 = b1; b2 < mc.numBuckets[s2]; b2++) {
                if ((s1 == s2) && (b1 == b2)) { continue; }
                o = bucketsHaveACommonBlock(&(mc.bucket[s1][b1]), 
                                                  &(mc.bucket[s2][b2]));
                compareFullFilters(mc.bucket[s1][b1].bp, 
                                              mc.bucket[s2][b2].bp, &c);
                overlap = (int) ((float) c.overlap * (float) 1.5625);
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
      }
      printMtmCluster(&mc);
      // report the results
      total = vlow_index[0]+low_index[0]+high_index[0]+vhigh_index[0]+
              vlow_index[1]+low_index[1]+high_index[1]+vhigh_index[1];
      for (o = 0; o < 2; o++) {
        getStatsInt(&vlowS[o], vlow[o], vlow_index[o]);
        getStatsInt(&lowS[o], low[o], low_index[o]);
        getStatsInt(&highS[o], high[o], high_index[o]);
        getStatsInt(&vhighS[o], vhigh[o], vhigh_index[o]);
        fprintf(as->f, "%d %d %d %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",
                   o, as->numLeftBuckets, as->numRightBuckets, 
                   as->numBaseBlocks,
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
  freeBucket(vbp);
}

do_getSegregateMask(bucket *userList, int numSamples, int numChildren)
{
  int mask, max_bsize, shift;
  int i, j;
  int max=0, min=100000000;
  bucket *bp;

  getSegregateMask(numSamples, numChildren, &mask, &max_bsize, &shift);
  printf("\n%d samples, %d children, mask %x (%d), max_bsize %d, shift %d\n",
                     numSamples, numChildren, mask, mask, max_bsize, shift);
  printf("Expect %d users per bucket\n", 
         (int)((float) USER_LIST_SIZE / (float)mask));

  for (i = 0; i < numSamples; i++) {
    for (j = 0; j < numChildren; j++) {
      bp = makeSegregatedBucketFromList(mask, shift, max_bsize, i, j,
                             0x38447262, userList, USER_LIST_SIZE);
      max = (bp->bsize > max)?bp->bsize:max;
      min = (bp->bsize < min)?bp->bsize:min;
    }
  }
  printf("max bucket %d, min bucket %d\n", max, min);
}

test_getSegregateMask(bucket *userList)
{
  int numSamples, numChildren;

  numSamples = 10; numChildren = 2;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 20; numChildren = 2;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 80; numChildren = 16;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 160; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 320; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 640; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);
}
