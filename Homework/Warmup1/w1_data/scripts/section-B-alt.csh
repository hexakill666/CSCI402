#!/bin/tcsh -f

echo '(B) Just generate printout without running "diff"...'

                # /bin/rm -f f?.sort f??.sort
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14)
                    echo "===> w1data/f$f"
                    ./warmup1 sort w1data/f$f > f$f.sort
                    # diff w1data/f$f.sort f$f.sort
                end

                # /bin/rm -f f?.sort f??.sort
                foreach f (15 16 17 18 19 20 21 22 23 24 25 26 27 28 29)
                    echo "===> w1data/f$f"
                    cat w1data/f$f | ./warmup1 sort > f$f.sort
                    # diff w1data/f$f.sort f$f.sort
                end
                # /bin/rm -f f?.sort f??.sort

echo "(B) Check printout one at a time..."

                # /bin/rm -f f?.sort f??.sort
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14)
                    echo -n 'Press any key to run "'diff w1data/f$f.sort f$f.sort'"...'
                    set junk="$<"
                    diff w1data/f$f.sort f$f.sort
                end

                # /bin/rm -f f?.sort f??.sort
                foreach f (15 16 17 18 19 20 21 22 23 24 25 26 27 28 29)
                    echo -n 'Press any key to run "'diff w1data/f$f.sort f$f.sort'"...'
                    set junk="$<"
                    diff w1data/f$f.sort f$f.sort
                end
                # /bin/rm -f f?.sort f??.sort

echo "(B) Delete all generated .sort files..."

                echo -n "Press any key to delete all generated .sort file (or press <Cntrl+C> to abort)..."
                set junk="$<"
                foreach f (0 1 2 3 4 5 6 7 8 9 10 11 12 13 14)
                    /bin/rm -f f$f.sort
                end

                foreach f (15 16 17 18 19 20 21 22 23 24 25 26 27 28 29)
                    /bin/rm -f f$f.sort
                end
