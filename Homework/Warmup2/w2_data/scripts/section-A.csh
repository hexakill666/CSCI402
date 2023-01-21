#!/bin/tcsh -f

echo "(A) Basic running of the code ..."

    /bin/rm -f f?.out f?.ana

    echo "===> test_0"
    ./warmup2 -B 3 -t w2data/f0.txt > f0.out
    ./analyze-trace.txt -B 3 -t w2data/f0.txt f0.out > f0.ana
    echo "===> test_1"
    ./warmup2 -t w2data/f1.txt -B 2 > f1.out
    ./analyze-trace.txt -t w2data/f1.txt -B 2 f1.out > f1.ana
    echo "===> test_2"
    ./warmup2 -B 1 -t w2data/f2.txt > f2.out
    ./analyze-trace.txt -B 1 -t w2data/f2.txt f2.out > f2.ana
    echo "===> test_3"
    ./warmup2 -r 4.5 -t w2data/f3.txt > f3.out
    ./analyze-trace.txt -r 4.5 -t w2data/f3.txt f3.out > f3.ana
    echo "===> test_4"
    ./warmup2 -r 5 -B 2 -t w2data/f4.txt > f4.out
    ./analyze-trace.txt -r 5 -B 2 -t w2data/f4.txt f4.out > f4.ana
    echo "===> test_5"
    ./warmup2 -B 2 -t w2data/f5.txt -r 15 > f5.out
    ./analyze-trace.txt -B 2 -t w2data/f5.txt -r 15 f5.out > f5.ana
    echo "===> test_6"
    ./warmup2 -t w2data/f6.txt -r 25 -B 2 > f6.out
    ./analyze-trace.txt -t w2data/f6.txt -r 25 -B 2 f6.out > f6.ana
    echo "===> test_7"
    ./warmup2 -t w2data/f7.txt -B 1 -r 5 > f7.out
    ./analyze-trace.txt -t w2data/f7.txt -B 1 -r 5 f7.out > f7.ana
    echo "===> test_8"
    ./warmup2 -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5 > f8.out
    ./analyze-trace.txt -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5 f8.out > f8.ana
    echo "===> test_9"
    ./warmup2 -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15 > f9.out
    ./analyze-trace.txt -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15 f9.out > f9.ana

