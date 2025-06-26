#ifndef __SIMULATIONH_H__
#define __SIMULATIONH_H__

#include "graph.h"
#include "array.h"

typedef struct {
        float * packetDeliveredBetweenStates;
        float * packetSentBetweenStates;
		float * timeBetweenStates;
} t_optimal_cycle;

typedef struct {
		float * meanIntervalPerFlow;
		float * packetsLossPerFlow;
        //float * packetDeliveredPerFlow;
        //float * packetSentPerFlow;
		float * delayFlows;
		//float meanInterval;
        float cost;
        float delay;
        float * rateFlows;
        t_optimal_cycle * optimal;
} t_return;

t_return * simulationSimulate(t_graph * graph, t_array * paths, t_array * flowTimes, t_array * txDurations);

#endif

