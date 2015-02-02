#!/bin/bash

gcc -o runTests filters.c runTests.c utilities.c -lm
#gcc -v -da -Q -o runAttacks filters.c attacks.c defenses.c utilities.c -lm
#gcc -o runAttacks filters.c attacks.c defenses.c utilities.c -lm
gcc -g -gstabs -O0 -o runAttacks filters.c attacks.c defenses.c utilities.c -lm
