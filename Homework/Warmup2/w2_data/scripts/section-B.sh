#!/bin/bash

echo "(B) Basic running of the code with <Cntrl+C> ..."

argc=$#

if [ $argc -ne 1 ]
then
    echo "usage: ./section-B.sh [0-9]"
    exit 1
fi

arg=$1

case $arg in
  0)
    ./warmup2 -B 3 -t w2data/f0.txt
    ;;
  1)
    ./warmup2 -t w2data/f1.txt -B 2
    ;;
  2)
    ./warmup2 -B 1 -t w2data/f2.txt
    ;;
  3)
    ./warmup2 -r 4.5 -t w2data/f3.txt
    ;;
  4)
    ./warmup2 -r 5 -B 2 -t w2data/f4.txt
    ;;
  5)
    ./warmup2 -B 2 -t w2data/f5.txt -r 15
    ;;
  6)
    ./warmup2 -t w2data/f6.txt -r 25 -B 2
    ;;
  7)
    ./warmup2 -t w2data/f7.txt -B 1 -r 5
    ;;
  8)
    ./warmup2 -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5
    ;;
  9)
    ./warmup2 -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15
    ;;
  *)
    echo "usage: ./section-B.sh [0-9]"
    ;;
esac
