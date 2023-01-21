#!/bin/tcsh -f

echo "(B) Basic running of the code with <Cntrl+C> ..."

foreach f ( $argv )
  switch ( $f )
  case "0":
    ./warmup2 -B 3 -t w2data/f0.txt
    breaksw
  case "1":
    ./warmup2 -t w2data/f1.txt -B 2
    breaksw
  case "2":
    ./warmup2 -B 1 -t w2data/f2.txt
    breaksw
  case "3":
    ./warmup2 -r 4.5 -t w2data/f3.txt
    breaksw
  case "4":
    ./warmup2 -r 5 -B 2 -t w2data/f4.txt
    breaksw
  case "5":
    ./warmup2 -B 2 -t w2data/f5.txt -r 15
    breaksw
  case "6":
    ./warmup2 -t w2data/f6.txt -r 25 -B 2
    breaksw
  case "7":
    ./warmup2 -t w2data/f7.txt -B 1 -r 5
    breaksw
  case "8":
    ./warmup2 -n 8 -r 3 -B 7 -P 5 -lambda 3.333 -mu 0.5
    breaksw
  case "9":
    ./warmup2 -mu 0.7 -r 2.5 -P 2 -lambda 2.5 -B 7 -n 15
    breaksw
  default:
    echo "usage: ./section-B.csh NUMBER"
  endsw
  exit 0
end
