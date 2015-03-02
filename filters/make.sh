#!/bin/bash

gcc -g -o runTests hightouch.c buckets.c filters.c runTests.c utilities.c -lm
echo "runTests compiled"

gcc -g -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
echo "runAttacks compiled"

gcc -g -o computeOverlap computeOverlap.c hightouch.c buckets.c filters.c utilities.c -lm
echo "computeOverlap compiled"



#gcc -v -da -Q -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
