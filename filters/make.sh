#!/bin/bash

gcc -o runTests filters.c runTests.c utilities.c -lm
gcc -o runAttacks filters.c attacks.c defenses.c utilities.c -lm
