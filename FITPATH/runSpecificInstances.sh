#!/bin/bash

if [ $# -ne 2 ]
then
	echo "Use: $0 <topology> <inst_dir>"
	exit 1
fi

TOPOFILE=$1
INSTDIR=$2

INPUTFILE=$(mktemp /tmp/SPF3_XXX)

# Generate sets of flows and run programs
for i in $INSTDIR/*.inst
do

		cat $TOPOFILE $i > $INPUTFILE
#		echo "Optimum"
#		./optimum -p $PREFIX $INPUTFILE
		echo "Heuristic1"
		./heuristic1 $INPUTFILE
		echo "Heuristic2"
		./heuristic2 $INPUTFILE
		echo "Heuristic2_5"
		./heuristic2_5 $INPUTFILE
		echo "Heuristic1b"
		./heuristic1b $INPUTFILE
		echo "Heuristic2b"
		./heuristic2b $INPUTFILE
		echo "Heuristic2_5b"
		./heuristic2_5b $INPUTFILE

done

rm -f $INPUTFILE

