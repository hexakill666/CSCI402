#!/bin/bash

echo "(A) Basic running of the code ..."

/bin/rm -f f?.out f?.ana

for i in {0..9}
do
  case $i in
    0)
      echo "===> test_0 (minimum emulation time is 26.5 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -B 3 -t w2data/f0.txt > f0.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -B 3 -t w2data/f0.txt f0.out > f0.ana
      ;;
    1)
      echo "===> test_1 (minimum emulation time is 23.0 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -t w2data/f1.txt -B 2 > f1.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -t w2data/f1.txt -B 2 f1.out > f1.ana
      ;;
    2)
      echo "===> test_2 (minimum emulation time is 13.1 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -B 1 -t w2data/f2.txt > f2.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -B 1 -t w2data/f2.txt f2.out > f2.ana
      ;;
    3)
      echo "===> test_3 (minimum emulation time is 16.8 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -r 4.5 -t w2data/f3.txt > f3.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -r 4.5 -t w2data/f3.txt f3.out > f3.ana
      ;;
    4)
      echo "===> test_4 (minimum emulation time is 18.0 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -r 5 -B 2 -t w2data/f4.txt > f4.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -r 5 -B 2 -t w2data/f4.txt f4.out > f4.ana
      ;;
    5)
      echo "===> test_5 (minimum emulation time is 20.3 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -B 2 -t w2data/f5.txt -r 15 > f5.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -B 2 -t w2data/f5.txt -r 15 f5.out > f5.ana
      ;;
    6)
      echo "===> test_6 (minimum emulation time is 14.1 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -t w2data/f6.txt -r 25 -B 2 > f6.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -t w2data/f6.txt -r 25 -B 2 f6.out > f6.ana
      ;;
    7)
      echo "===> test_7 (minimum emulation time is 15.1 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -t w2data/f7.txt -B 1 -r 5 > f7.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -t w2data/f7.txt -B 1 -r 5 f7.out > f7.ana
      ;;
    8)
      echo "===> test_8 (minimum emulation time is 15.3 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5 > f8.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5 f8.out > f8.ana
      ;;
    9)
      echo "===> test_9 (minimum emulation time is 13.4 seconds)"
      echo -n "     start time: "; date
      ./warmup2 -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15 > f9.out
      echo -n "       end time: "; date
      ./analyze-trace.txt -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15 f9.out > f9.ana
      ;;
    *)
      echo "usage: ./section-A-all.sh"
      ;;
  esac
done

/bin/ls -lrt f?.???
