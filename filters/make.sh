#!/bin/bash

#gcc -g -o runTests clusters.c hightouch.c buckets.c filters.c runTests.c utilities.c -lm
echo "runTests done"
echo "**********************************************************"

gcc -g -o runAttacks clusters.c hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
echo "runAttacks done"
echo "**********************************************************"

#gcc -g -o computeOverlap clusters.c computeOverlap.c hightouch.c buckets.c filters.c utilities.c -lm
echo "computeOverlap done"



#gcc -v -da -Q -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
