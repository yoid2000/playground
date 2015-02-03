#!/bin/bash

DIR=$(pwd)
OUTDIR="/home/francis/gnuplot/computeOverlap"

for SizeRatioMax in 1 2 4 8 16
do
  ./computeOverlap 2000 4 $SizeRatioMax $SizeRatioMax $OUTDIR
  ./makeGnuPlot.sh 2000 4 $SizeRatioMax $SizeRatioMax $OUTDIR
done
