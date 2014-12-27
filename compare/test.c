
#include <stdio.h>

#define MASK_ARRAY_SIZE 59
#define LAST_MASK MASK_ARRAY_SIZE-1

unsigned long masks[] = {
  0xfc00000000000000,
  0x7e00000000000000,
  0x3f00000000000000,
  0x1f80000000000000,
  0xfc0000000000000,
  0x7e0000000000000,
  0x3f0000000000000,
  0x1f8000000000000,
  0xfc000000000000,
  0x7e000000000000,
  0x3f000000000000,
  0x1f800000000000,
  0xfc00000000000,
  0x7e00000000000,
  0x3f00000000000,
  0x1f80000000000,
  0xfc0000000000,
  0x7e0000000000,
  0x3f0000000000,
  0x1f8000000000,
  0xfc000000000,
  0x7e000000000,
  0x3f000000000,
  0x1f800000000,
  0xfc00000000,
  0x7e00000000,
  0x3f00000000,
  0x1f80000000,
  0xfc0000000,
  0x7e0000000,
  0x3f0000000,
  0x1f8000000,
  0xfc000000,
  0x7e000000,
  0x3f000000,
  0x1f800000,
  0xfc00000,
  0x7e00000,
  0x3f00000,
  0x1f80000,
  0xfc0000,
  0x7e0000,
  0x3f0000,
  0x1f8000,
  0xfc000,
  0x7e000,
  0x3f000,
  0x1f800,
  0xfc00,
  0x7e00,
  0x3f00,
  0x1f80,
  0xfc0,
  0x7e0,
  0x3f0,
  0x1f8,
  0xfc,
  0x7e,
  0x3f,
  0x0,
};

doDiff(unsigned long v1, unsigned long v2) {
  unsigned long *first = &masks[0];
  unsigned long *last = &masks[LAST_MASK];
  unsigned long *highp = first;
  unsigned long *lowp = first;
  int i, j;
  int numMatches = 0;

  while(1) {
    for (i = 0; i < 4; i++) {
      if ( zzzz
    }
  }

  printf("v1 is %lx, v2 is %lx\n", v1, v2);
}

main() 
{
  doDiff(0x1111111111111111, 0x1111111111110000);
}
