#!/bin/bash

gcc -g -o runTests hightouch.c buckets.c filters.c runTests.c utilities.c -lm
#gcc -v -da -Q -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
gcc -g -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
gcc -g -o computeOverlap computeOverlap.c hightouch.c buckets.c filters.c utilities.c -lm
