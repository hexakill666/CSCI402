#!/bin/tcsh -f

echo "Do this first..."

if (-f my402list.h) then
    /bin/mv my402list.h my402list.h.submitted
endif
if (-f cs402.h) then
    /bin/mv cs402.h cs402.h.submitted
endif
/bin/cp w1data/cs402.h .
/bin/cp w1data/my402list.h .
make warmup1

