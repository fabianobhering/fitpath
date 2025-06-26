#make heuristicILS_mate
for inst in $(seq 1 1);
do
   #./heuristicILS_ref1 ../inst/links/4s-links300_60-$inst-ref1 1 $inst
   #./heuristicILS_ref2 ../inst/links/4s-links300_60-$inst-ref2 2 $inst
   #./heuristicILS_ref3 ../inst/links/4s-links300_60-$inst-ref3 3 $inst
   #./heuristicILS_after ../inst/links/4s-links300_60-$inst-ref6 6 $inst
   #./heuristicILS_mate /local/cluster04/fabianobhering/exp2021/inst/links/4s-links300_60-$inst-ref0 0 $inst
   #./heuristicILS_mate /local/cluster04/fabianobhering/exp2021/inst/links/links300_60-$inst-ref7 7 $inst
   #./heuristicILS_mate ../inst/links/4s-links300_60-$inst-ref7 7 $inst
   #./heuristicILS_mate ../inst/links/4s-links300_30_$inst_ref1 1 $inst 30
   #./heuristicILS_mate ../inst/links/hall-60/w-htc/4s-256-cl/4s-links300_60-10-ref0 60 $inst 0
   ./heuristicILS_mate ../inst/links/bus-60/w-htc/2s-1_5M/2s-links300_60-1-ref0-400 60 $inst 0
done
