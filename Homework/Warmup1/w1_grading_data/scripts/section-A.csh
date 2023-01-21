#!/bin/tcsh -f

echo "(A) Doubly-linked Circular List..."

                /bin/rm -rf grading_$$
                mkdir grading_$$
                cd grading_$$
                cp ../my402list.c .
                cp ../w1data/cs402.h .
                cp ../w1data/my402list.h .
                cp ../w1data/listtest.c .
                cp ../w1data/Makefile .
                make

                #
                # these values will be changed for grading
                #
                set seeds = ( 91 90 89 88 87 86 85 84 83 82 81 80 79 78 77 76 75 74 73 72 71 )

                #
                # for the following commands, each correct behavior gets 2 point
                # gets 2 points if "./listtest" command produces NOTHING
                # gets 0 point if "./listtest" command produces ANY output
                #
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19)
                    @ index = $f + 1
                    echo "===> test_$f"
                    ./listtest -seed=$seeds[$index]
                end
                cd ..

                #
                # Clean up temporary directory
                #
                /bin/rm -rf grading_$$
