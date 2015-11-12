#!/bin/sh
#DEBUG with set -x
CMD=./$1
ERROR=0
PASS=1
THREAD_COUNT_LIST="1 2 3 4 5 6 78 100 200 8000"

test_on() {
    echo "Pass $PASS ($1). \c"
    for tc in $THREAD_COUNT_LIST
    do
        echo "$tc  \c"
        LOG=/tmp/$$.$tc.log
        RES=/tmp/$$.$tc.res
        EXEC="$CMD $3 $tc"
        $EXEC > $RES 2> $LOG
        R=`grep Found $LOG | cut -f2 -d\:`
        if [ "$R" != " Found $2 words" ]
        then
            ERROR=`expr $ERROR + 1`
            echo "\"$EXEC\" produced an error see $RES and $LOG"
        else
            rm $LOG $TMP
        fi
    done
    PASS=`expr $PASS + 1`
    echo
}
test_on short `wc -w lorem48`
test_on short `wc -w lorem5000`

# Much huger test
cp lorem5000 /tmp/loremtest
for i in 1 2 3 4 5 6
do
    cat /tmp/loremtest /tmp/loremtest > /tmp/loremtest2
    cat /tmp/loremtest2 /tmp/loremtest2 > /tmp/loremtest
done
rm /tmp/loremtest2
test_on long `wc -w /tmp/loremtest`
#rm /tmp/loremtest
if [ $ERROR == 0 ]
then
    echo "Tests ok."
else
    echo "Tests failed."
fi
exit $ERROR

