#!/bin/bash

cat $1  | awk '
BEGIN{

	mode="o";
}

/Source/{

	getline;
	src = $0;
	while(1) {

		getline;
		if ($1 + 0 != $1) break ;
		src = src "," $0;
	}
}

/Destination/{

	getline;
	dst = $0;
	while(1) {

		getline;
		if ($1 + 0 != $1) break ;
		dst = dst "," $0;
	}
}

/Best path/{

	if (mode == "o") {

		optCost = $6;
		getline;
		optPath = $0;
		mode = "h";
	}
	else {

		heuCost = $6;
		getline;
		heuPath = $0;
		if (heuCost != optCost) 
			printf "%9s | %9s | %5.2f | %40s | %5.2f | %40s | False\n", src, dst, optCost, optPath, heuCost, heuPath;
		else
			printf "%9s | %9s | %5.2f | %40s | %5.2f | %40s | True\n", src, dst, optCost, optPath, heuCost, heuPath;
		mode = "o";
		dst = "";
	}
}'

