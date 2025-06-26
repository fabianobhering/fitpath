#!/bin/bash

if [ $# -ne 3 ]
then
	echo "Use: $0 <topology> <# of flows> <prefix>"
	exit 1
fi

TOPOFILE=$1
NFLOWS=$2
PREFIX=$3


# Find out the number of nodes in the topology (we assume
# nodes are numbered from 0 to a nnodes - 1).
NNODES=$(cat $TOPOFILE | awk '/Links/{next;} {if ($1 >= nnodes) nnodes = $1 + 1; if ($2 >= nnodes) nnodes = $2 + 1;} END{print nnodes}')
echo $NNODES

INPUTFILE=$(mktemp /tmp/SPF1_XXX)
FLOWFILE=$(mktemp /tmp/SPF2_XXX)
TMPFILE=$(mktemp /tmp/SPF3_XXX)

#INPUTFILE=/tmp/SPF1_ocC 
#FLOWFILE=/tmp/SPF2_tRe 
#TMPFILE=/tmp/SPF3_1OO

touch $INPUTFILE $FLOWFILE

# Generate sets of flows and run programs
while [ 1 ]
do
	cat $FLOWFILE | awk -v nflows=$NFLOWS -v nnodes=$NNODES '

		/Source/{

			nsources = 0;
			nonempty = 1;
			mode = "s";
			next ;
		}
		/Destination/{
			
			ndests = 0;
			nonempty = 1;
			mode = "d";
			next ;
		}
		{
			if (mode == "s") {
			
				sources[nsources++] = $1;
			}
			else {

				dests[ndests++] = $1;
			}
		}
		END{

			if (nonempty == 0) {

				for (i = 0; i < nflows; i++) {

					dests[i] = 1;
					sources[i] = 0;
				}
			}
			if (nonempty || nflows > 1) {

				while(1) {

					for (i = 0; i < nflows; i++) {

						dests[i]++;
						if (dests[i] == sources[i]) dests[i]++;
						if (dests[i] >= nnodes) {
						
							dests[i] = 0;
							sources[i]++;
							if (sources[i] >= nnodes) {

								if (i == nflows - 1) exit;
								dests[i] = 1
								sources[i] = 0;
#								i = i - 1;
								continue ;
							}
						}

						break ;
					}
					
					for (i = 0; i < nflows; i++) {
						for (j = 0; j < nflows; j++) {
					
							if (i == j) continue ;
							if (sources[i] == sources[j] && dests[i] == dests[j]) break ;
						}
					}

					if (i == nflows && j == nflows) break ;
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
		cp $TMPFILE $FLOWFILE
		cat $TOPOFILE $FLOWFILE > $INPUTFILE
		cat $FLOWFILE
#		echo "Optimum"
#		./optimum -p $PREFIX $INPUTFILE
		echo "Heuristic1(1)"
		./heuristic1 $INPUTFILE 1
		echo "Heuristic1(2)"
		./heuristic1 $INPUTFILE 2
		echo "Heuristic1(3)"
		./heuristic1 $INPUTFILE 3
		echo "Heuristic1(4)"
		./heuristic1 $INPUTFILE 4 
		echo "Heuristic1(5)"
		./heuristic1 $INPUTFILE 5
		echo "Heuristic1(6)"
		./heuristic1 $INPUTFILE 6
		echo "Heuristic1(7)"
		./heuristic1 $INPUTFILE 7
		echo "Heuristic1(8)"
		./heuristic1 $INPUTFILE 8
		echo "Heuristic1(9)"
		./heuristic1 $INPUTFILE 9
		echo "Heuristic1(10)"
		./heuristic1 $INPUTFILE 10
#		echo "Heuristic2"
#		./heuristic2 $INPUTFILE
#		echo "Heuristic2_5"
#		./heuristic2_5 $INPUTFILE
#		echo "Heuristic1b"
#		./heuristic1b $INPUTFILE
#		echo "Heuristic2b"
#		./heuristic2b $INPUTFILE
#		echo "Heuristic2_5b"
#		./heuristic2_5b $INPUTFILE
#		./evaluateSimulation2 $INPUTFILE
done

rm -f $INPUTFILE $FLOWFILE $TMPFILE

