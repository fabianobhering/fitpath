#!/bin/bash

if [ $# -ne 4 ]
then
	echo "Use: $0 <topology> <# of flows> <# of instances> <prefix>"
	exit 1
fi

TOPOFILE=$1
NFLOWS=$2
NINSTANCES=$3
PREFIX=$4

# Find out the number of nodes in the topology (we assume
# nodes are numbered from 0 to a nnodes - 1).
NNODES=$(cat $TOPOFILE | awk '/Links/{next;} {if ($1 >= nnodes) nnodes = $1 + 1; if ($2 >= nnodes) nnodes = $2 + 1;} END{print nnodes}')
echo $NNODES

INPUTFILE=$(mktemp /tmp/SPF1_XXX)
FLOWFILE=$(mktemp /tmp/SPF2_XXX)
TMPFILE=$(mktemp /tmp/SPF3_XXX)

TMPFILE2=$(mktemp /tmp/SPF3_XXX)
TMPFILE3=$(mktemp /tmp/SPF3_XXX)
TMPFILE4=$(mktemp /tmp/SPF3_XXX)
TMPFILE5=$(mktemp /tmp/SPF3_XXX)
TMPFILE6=$(mktemp /tmp/SPF3_XXX)
TMPFILE7=$(mktemp /tmp/SPF3_XXX)

touch $INPUTFILE $FLOWFILE

# Generate sets of flows and run programs
for i in $(seq 1 $NINSTANCES)
do
	echo | awk -v nflows=$NFLOWS -v nnodes=$NNODES '

		END {	
			srand();
			for (i = 0; i < nflows; i++) {

				while(1) {

					newSrc = int(rand() * nnodes);
					newDst = int(rand() * nnodes);
					if (newSrc == newDst) continue;

					for (j = 0; j < i; j++) {

						if (sources[j] == newSrc && dests[j] == newDst) break ;
					}

					if (j == i) {

						sources[i] = newSrc;
						dests[i] = newDst;
						break ;
					}
				}
			}

			print "Source"
			for (i = 0; i < nflows; i++) print sources[i];
			print "Destination"
			for (i = 0; i < nflows; i++) print dests[i];

		}' > $TMPFILE
		if [ ! -s $TMPFILE ] 
		then
			break
		fi
		echo "Done generating."
		cp $TMPFILE $FLOWFILE
		cat $TOPOFILE $FLOWFILE > $INPUTFILE
		cat $FLOWFILE
#		echo "Optimum"
#		./optimum -p $PREFIX $INPUTFILE
		sleep 1
		./heuristic1 $INPUTFILE > $TMPFILE2 &
		./heuristic2 $INPUTFILE > $TMPFILE3 &
		./heuristic2_5 $INPUTFILE > $TMPFILE4 & 
		./heuristic1 $INPUTFILE > $TMPFILE5 & 
		./heuristic2b $INPUTFILE > $TMPFILE6 & 
		./heuristic2_5b $INPUTFILE > $TMPFILE7 &
		wait
		echo "Heuristic1"
		cat $TMPFILE2 
		echo "Heuristic2"
		cat $TMPFILE3 
		echo "Heuristic2_5"
		cat $TMPFILE4 
		echo "Heuristic1b"
		cat $TMPFILE5 
		echo "Heuristic2b"
		cat $TMPFILE6 
		echo "Heuristic2_5b"
		cat $TMPFILE7
done

rm -f $INPUTFILE $FLOWFILE $TMPFILE

