#!/bin/bash

cat $1 | awk '
BEGIN{

	nnodes = 0;
	nlinks = 0;
	minx = 0;
	miny = 0;
	maxx = 0;
	maxy = 0;
	xmar = 20;
	ymar = 10;

	NODE_RADIO = 30;
	PACKET_WIDTH = 30;
	PACKET_HEIGHT = 20;
	INTRA_PACKET_SPACE = 3;
	CLOCK_X = 10;
	CLOCK_Y = 20;
	LABEL_FONT_SIZE = 14;
}

($1 == "Node") {

	NodeId[nnodes] = $2;
	NodeX[nnodes] = $3;
	NodeY[nnodes] = $4;
	NodeQueueX[nnodes] = $5;
	NodeQueueY[nnodes] = $6;
	NodeArrivedX[nnodes] = $7;
	NodeArrivedY[nnodes] = $8;
	Node2Idx[$2] = nnodes;
	nnodes++;
	multiplier = 0.05;

	if ($3 + NODE_RADIO > maxx) maxx = $3 + NODE_RADIO;
	if ($3 - NODE_RADIO < minx) minx = $3 - NODE_RADIO;
	if ($5 > maxx) maxx = $3;
	if ($5 < minx) minx = $3;

	if ($4 + NODE_RADIO > maxy) maxy = $4 + NODE_RADIO;
	if ($4 - NODE_RADIO < miny) miny = $4 - NODE_RADIO;
	if ($6 > maxy) maxy = $6;
	if ($6 < miny) miny = $6;
}

($1 == "Link") {

	links[nlinks++] = $2 "," $3 "," $4;
	if ($5 == "?") flowDest[$4] = $3;
}

($1 == "At") {

	split($2, a, ":");
	time = a[1];
	rest =  a[2];

	if (cicleBegin == "") {

		if (cicle[rest] == "")
			cicle[rest] = time;
		else
			cicleBegin = cicle[rest];
		cicleEnd = time;
	}

	if (time > maxtime) maxtime = time;

	timeEvents = timeEvents "," time;

	split(rest, a, "?");
	transmissions = a[1];
	queues = a[2];

	for (i in linkTransmitting) {

		if (linkTransmitting[i] != "") {

			split(linkTransmitting[i], b, ",");
			if (b[2] == time) {
	
				currentPkt = b[1];
				split(links[i], c, ",");

				if (flowDest[packetColor[currentPkt]] == c[2]) {

					if (arrived[c[2]] == "") arrived[c[2]] = currentPkt;
					else arrived[c[2]] = arrived[c[2]] "," currentPkt;
					bufSize = split(arrived[c[2]], z, ",");
					packets[currentPkt] = packets[currentPkt] "A|" (c[2] * 1) "|" time "|" bufSize ";";
					if (bufSize * 13 - NodeArrivedY[nodeIdx] > miny) maxy = bufSize * 13 - NodeQueueY[Node2Idx[c[2]]];
				}
				else {

					if (queue[c[2]] == "")
						queue[c[2]] = currentPkt;
					else
						queue[c[2]] = queue[c[2]] "," currentPkt;

					bufSize = split(queue[c[2]], z, ",");
					if (bufSize * 13 + NodeQueueY[nodeIdx] > maxy) maxy = bufSize * 13 + NodeQueueY[Node2Idx[c[2]]];
					packets[currentPkt] = packets[currentPkt] "Q|" (c[2] * 1) "|" time "|" bufSize ";";
				}

				linkTransmitting[i] = "";
			}
		}

	}

	ntransmissions = split(transmissions, a, ";") - 1;
	for (i = 1; i <= ntransmissions; i++) {

		split(a[i], b, "|");
		if (linkTransmitting[b[1]] == "") {

			split(links[b[1]], c, ",");
			if (queue[c[1]] != "") {

				queueSize = split(queue[c[1]], d, ",");
				newQueue = "";
				for (j = 1; j <= queueSize; j++) {

					if (packetColor[d[j]] == c[3]) break ;
					if (newQueue == "") newQueue = d[j];
					else newQueue = newQueue "," d[j];
				}
				if (j <= queueSize) {

					currentPkt = d[j];
					for (j++; j <= queueSize; j++) {
						
						packets[d[j]] = packets[d[j]] "Q|" (c[1] * 1) "|" time "|" (j - 1) ";";
						if (newQueue == "") newQueue = d[j];
						else newQueue = newQueue "," d[j];
					}
					queue[c[1]] = newQueue;
				}
				else {

					currentPkt = npackets++;
					packetColor[currentPkt] = c[3];
				}
			}
			else {

				currentPkt = npackets++;
				packetColor[currentPkt] = c[3];
			}

			linkTransmitting[b[1]] = currentPkt "," (b[2] + time);
			if (time + b[2] > maxtime) maxtime = time + b[2];
			if (packets[currentPkt] == "") 
				packets[currentPkt] = "T|" links[b[1]] "|" time "|" (b[2] + time) ";";
			else 
				packets[currentPkt] = packets[currentPkt] "T|" links[b[1]] "|" time "|" (b[2] + time) ";";
		}
	}
}

END{

	minx -= xmar;
	miny -= ymar;
	maxx += xmar;
	maxy += ymar;

	printf "<?xml version=\"1.0\" standalone=\"no\"?>\n";
	printf "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
	printf "<svg viewBox=\"%d %d %d %d\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" onload=\"Init(evt)\">\n", minx, miny, maxx - minx, maxy - miny;


	printf "<script><![CDATA[\n";
	printf "\tvar SVGDocument = null;\n";
	printf "\tvar SVGRoot = null;\n";
	printf "\tvar svgns = \"http://www.w3.org/2000/svg\";\n";
	printf "\tvar xlinkns = \"http://www.w3.org/1999/xlink\";\n";
	printf "\tvar svg = document.getElementsByTagName(\"svg\")[0];\n";
	printf "\tvar MAXTIME = %f;\n", maxtime * multiplier;
	printf "\n";

	printf "\tvar pauseButton = null;\n";
	printf "\tvar playButton = null;\n";
	printf "\tvar playing = 1;\n";
	printf "\n";

	printf "\tfunction Init(evt)\n";
	printf "\t{\n";
	printf "\t\tSVGDocument = evt.target.ownerDocument;\n";
	printf "\t\tSVGRoot = SVGDocument.documentElement;\n";
	printf "\n";

	printf "\t\tpauseButton = SVGDocument.getElementById(\"pauseGroup\");\n";
	printf "\t\tplayButton = SVGDocument.getElementById(\"playGroup\");\n";
	printf "\n";

	printf "\t\tslideChoice = -1;\n";
	printf "\n";

	printf "\t\tadvance = 1;\n";
	printf "\t\tsetTimeout(\"advanceTimeline()\", 500);\n";
	printf "\t};\n";
	printf "\n";

	printf "\tfunction Pause()\n";
	printf "\t{\n";
	printf "\t\tadvance = -1;\n";
	printf "\t\tSVGRoot.pauseAnimations();\n";
	printf "\t\tpauseButton.setAttributeNS(null, \"display\", \"none\");\n";
	printf "\t\tplayButton.setAttributeNS(null, \"display\", \"inline\");\n";
	printf "\t\tplaying = 0;\n";
	printf "\t};\n";
	printf "\n";

	printf "\tfunction Play()\n";
	printf "\t{\n";
	printf "\t\tif (advance == -1) advance = 0;\n";
	printf "\t\telse if (advance == 0) {\n";
	printf "\n";

	printf "\t\t\tadvance = 1;\n";
	printf "\t\t\tsetTimeout(\"advanceTimeline()\", 500);\n";
	printf "\t\t}\n";
	printf "\t\tSVGRoot.unpauseAnimations();\n";
	printf "\t\tplayButton.setAttributeNS(null, \"display\", \"none\");\n";
	printf "\t\tpauseButton.setAttributeNS(null, \"display\", \"inline\");\n";
	printf "\t\tplaying = 1;\n";
	printf "\t};\n";
	printf "\n";

	printf "\tfunction startColorDrag(which)\n";
	printf "\t{\n";
	printf "\t\tendColorDrag();\n";
	printf "\t\tslideChoice = which;\n";
	printf "\t}\n";
	printf "\n";

	printf "\tfunction endColorDrag()\n";
	printf "\t{\n";
	printf "\t\tslideChoice = -1;\n";
	printf "\t}\n";
	printf "\n";

	printf "\tfunction doColorDrag( evt, which )\n";
	printf "\t{\n";
	printf "\t\tif (slideChoice < 0 || slideChoice != which)\n";
	printf "\t\t{\n";
	printf "\t\t\treturn;\n";
	printf "\t\t}\n";
	printf "\n";
	 
	printf "\t\tvar slide = SVGDocument.getElementById(\"slide\" + slideChoice);\n";
	printf "\t\tvar pt = svg.createSVGPoint();\n";
	printf "\t\tvar obj = evt.target;\n";
	printf "\n";

	printf "\t\tpt.x = evt.clientX;\n";
	printf "\t\tpt.y = evt.clientX;\n";
	printf "\n";

	printf "\t\tvar tpt = pt.matrixTransform(slide.getScreenCTM().inverse());\n";
	printf "\n";

	printf "\t\tif (tpt.x > 100) tpt.x = 100;\n";
	printf "\t\tif (tpt.x < 0) tpt.x = 0;\n";
	printf "\n";

	printf "\t\tvar slideKnob = SVGDocument.getElementById(\"slideKnob\" + slideChoice );\n";
	printf "\t\tslideKnob.setAttribute(\"cx\", tpt.x);\n";
	printf "\n";

	printf "\t\tvar slideFG = SVGDocument.getElementById(\"slideFG\" + slideChoice);\n";
	printf "\t\tvar oldPercent = slideFG.getAttribute(\"width\");\n";
	printf "\t\tslideFG.setAttribute(\"width\", tpt.x);\n";
	printf "\n";

	printf "\t\tvar i;\n";
	printf "\t\tvar animations = SVGDocument.getElementsByTagName(\"animate\");\n";
	printf "\t\tvar sets = SVGDocument.getElementsByTagName(\"set\");\n";
	printf "\n";

	printf "\t\tvar t;\n";
	printf "\t\tfor (i = 0; i < animations.length; i++) {\n";
	printf "\t\t\tt = parseFloat(animations[i].getAttribute(\"begin\")) * oldPercent / tpt.x;\n";
	printf "\t\t\tanimations[i].setAttribute(\"begin\", t);\n";
	printf "\t\t\tt = parseFloat(animations[i].getAttribute(\"dur\")) * oldPercent / tpt.x;\n";
	printf "\t\t\tanimations[i].setAttribute(\"dur\", t);\n";
	printf "\t\t}\n";
	printf "\n";

	printf "\t\tfor (i = 0; i < sets.length; i++) {\n";
	printf "\n";

	printf "\t\t\tt = parseFloat(sets[i].getAttribute(\"begin\")) * oldPercent / tpt.x;\n";
	printf "\t\t\tsets[i].setAttribute(\"begin\", t);\n";
	printf "\t\t}\n";
	printf "\n";

	printf "\t\tMAXTIME *= oldPercent / tpt.x;\n";
	printf "\t\tt = SVGRoot.getCurrentTime() * oldPercent / tpt.x;\n";
	printf "\t\tSVGRoot.setCurrentTime(t);\n";
	printf "\t}\n";
	printf "\n";

	printf "\tfunction doColorDragTimeLine( evt, which )\n";
	printf "\t{\n";
	printf "\t\tif (slideChoice < 0 || slideChoice != which)\n";
	printf "\t\t{\n";
	printf "\t\t\treturn;\n";
	printf "\t\t}\n";
	printf "\n";
	    
	printf "\t\tvar slide = SVGDocument.getElementById(\"slide\" + slideChoice);\n";
	printf "\t\tvar pt = svg.createSVGPoint();\n";
	printf "\t\tvar obj = evt.target;\n";
	printf "\n";

	printf "\t\tpt.x = evt.clientX;\n";
	printf "\t\tpt.y = evt.clientX;\n";
	printf "\n";

	printf "\t\tvar tpt = pt.matrixTransform(slide.getScreenCTM().inverse());\n";
	printf "\n";

	printf "\t\tif (tpt.x > 100) tpt.x = 100;\n";
	printf "\t\tif (tpt.x < 0) tpt.x = 0;\n";
	printf "\n";

	printf "\t\tvar slideKnob = obj = SVGDocument.getElementById(\"slideKnob\" + slideChoice );\n";
	printf "\t\tslideKnob.setAttribute(\"cx\", tpt.x);\n";
	printf "\n";

	printf "\t\tvar slideFG = SVGDocument.getElementById(\"slideFG\" + slideChoice);\n";
	printf "\t\tslideFG.setAttribute(\"width\", tpt.x);\n";
	printf "\n";

	printf "\t\tSVGRoot.setCurrentTime(tpt.x * MAXTIME / 100);\n";
	printf "\t}\n";
	printf "\n";


	printf "\t\tfunction advanceTimeline() {\n";
	printf "\n";

	printf "\t\tif (advance == -1) {\n";
	printf "\n";
	
	printf "\t\t\tadvance = 0;\n";
	printf "\t\t\treturn ;\n";
	printf "\t\t}\n";
	printf "\n";

	printf "\t\tvar percent = (SVGRoot.getCurrentTime() * 100.0) / MAXTIME;\n";
	printf "\n";

	printf "\t\tif (percent > 100) {\n";
	printf "\n";

	printf "\t\t\tSVGRoot.setCurrentTime(%f);\n", cicleBegin * multiplier;
	printf "\t\t\tpercent = (SVGRoot.getCurrentTime() * 100.0) / MAXTIME;\n";
	printf "\t\t}\n";
	printf "\n";

	printf "\t\tvar slideKnob = obj = SVGDocument.getElementById(\"slideKnob2\");\n";
	printf "\t\tslideKnob.setAttribute(\"cx\", percent);\n";
	printf "\n";

	printf "\t\tvar slideFG = SVGDocument.getElementById(\"slideFG2\");\n";
	printf "\t\tslideFG.setAttribute(\"width\", percent);\n";
	printf "\n";

	printf "\t\tsetTimeout(\"advanceTimeline()\", 500);\n";
	printf "\t}\n";


	printf "\t]]></script>\n";
	printf "\n";

	printf "\t<a id=\"playGroup\" display=\"none\" onclick=\"Play()\">\n";
	printf "\t<circle id=\"play\" cx=\"50\" cy=\"50\" r=\"15\" fill=\"green\"/>\n";
	printf "\t<polygon points=\"46,44 57,50 46,56\" fill=\"lime\" stroke=\"lime\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n";
	printf "\t</a>\n\n";
	printf "\n";

	printf "\t<a id=\"pauseGroup\" display=\"inline\" onclick=\"Pause()\">\n";
	printf "\t<circle id=\"pause\" cx=\"50\" cy=\"50\" r=\"15\" fill=\"red\"/>\n";
	printf "\t<line x1=\"47\" y1=\"45\" x2=\"47\" y2=\"55\" stroke=\"pink\" stroke-width=\"4\" stroke-linecap=\"round\"/>\n";
	printf "\t<line x1=\"54\" y1=\"45\" x2=\"54\" y2=\"55\" stroke=\"pink\" stroke-width=\"4\" stroke-linecap=\"round\"/>\n";
	printf "\t</a>\n\n";
	printf "\n";

	printf "\t<defs>\n";
	printf "\t\t<linearGradient id=\"grad1\" x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\">\n";
	printf "\t\t<stop offset=\"0%\" style=\"stop-color:rgb(150,150,150);stop-opacity:1\" />\n";
	printf "\t\t<stop offset=\"50%\" style=\"stop-color:rgb(240,240,240);stop-opacity:1\" />\n";
	printf "\t\t<stop offset=\"100%\" style=\"stop-color:rgb(150,150,150);stop-opacity:1\" />\n";
	printf "\t\t</linearGradient>\n";
	printf "\t\t<radialGradient id=\"grad2\" cx=\"50%\" cy=\"50%\" r=\"75%\" fx=\"50%\" fy=\"50%\">\n";
	printf "\t\t<stop offset=\"0%\" style=\"stop-color:rgb(240,240,240);stop-opacity:1\" />\n";
	printf "\t\t<stop offset=\"100%\" style=\"stop-color:rgb(150,150,150);stop-opacity:1\" />\n";
	printf "\t\t</radialGradient>\n";
	printf "\t\t<linearGradient id=\"grad3\" x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"100%\">\n";
	printf "\t\t<stop offset=\"0%\" style=\"stop-color:rgb(10,15,105);stop-opacity:1\" />\n";
	printf "\t\t<stop offset=\"100%\" style=\"stop-color:rgb(15,15,250);stop-opacity:1\" />\n";
	printf "\t\t</linearGradient>\n";
	printf "\t</defs>\n";
	printf "\n";

	printf "\t<g id=\"slide1\" onmousedown=\"startColorDrag(1)\" onmouseup=\"endColorDrag()\" onmousemove=\"doColorDrag(evt,1)\" transform=\"translate(120, 45)\">\n";
	printf "\t\t<rect id=\"slideBG1\" x=\"0\" y=\"0\" width=\"100\" height=\"10\" rx=\"5\" ry=\"5\" fill=\"url(#grad1)\" stroke=\"gray\" stroke-width=\"1\"/>\n";
	printf "\t\t<rect id=\"slideFG1\" x=\"1\" y=\"1\" width=\"100\" height=\"8\" rx=\"5\" ry=\"5\" fill=\"url(#grad3)\"/>\n";
	printf "\t\t<circle id=\"slideKnob1\" cx=\"100\" cy=\"5\" r=\"8\" stroke=\"gray\" stroke-width=\"1\" fill=\"url(#grad2)\"/>\n";
	printf "\t\t<text x=\"15\" y=\"30\" font-size=\"12\">Velocidade</text> \n";
	printf "\t</g>\n";
	printf "\n";

	printf "\t<g id=\"slide2\" onmousedown=\"startColorDrag(2)\" onmouseup=\"endColorDrag()\" onmousemove=\"doColorDragTimeLine(evt,2)\" transform=\"translate(250, 45)\">\n";
	printf "\t\t<rect id=\"slideBG2\" x=\"0\" y=\"0\" width=\"100\" height=\"10\" rx=\"5\" ry=\"5\" fill=\"url(#grad1)\" stroke=\"gray\" stroke-width=\"1\"/>\n";
	printf "\t\t<rect id=\"slideFG2\" x=\"1\" y=\"1\" width=\"0\" height=\"8\" rx=\"5\" ry=\"5\" fill=\"url(#grad3)\"/>\n";
	printf "\t\t<circle id=\"slideKnob2\" cx=\"0\" cy=\"5\" r=\"8\" stroke=\"gray\" stroke-width=\"1\" fill=\"url(#grad2)\"/>\n";
	printf "\t\t<text x=\"15\" y=\"30\" font-size=\"12\">Andamento</text> \n";
	printf "\t</g>\n";

	printf "\n";
	for (i = 0; i < npackets; i++) {

		noperations = split(packets[i], operations, ";");
		color = "";
		for (j = 1; j < noperations; j++) {

			argc = split(operations[j], argv, "|");
			if (argv[1] == "T") {

				split(argv[2], a, ",");
				src = Node2Idx[a[1]];
				dst = Node2Idx[a[2]];
				if (color == "") {

					color = a[3];
					printf "\t<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\">\n", NodeX[src] - NODE_RADIO / 2, NodeY[src], PACKET_WIDTH, PACKET_HEIGHT, color;
				}
				
				printf "\t\t<animate attributeName=\"x\" attributeType=\"XML\" begin=\"%fs\" dur=\"%fs\" fill=\"freeze\" from=\"%d\" to=\"%d\" />\n", argv[3] * multiplier, (argv[4] - argv[3]) * multiplier, NodeX[src], NodeX[dst];
				printf "\t\t<animate attributeName=\"y\" attributeType=\"XML\" begin=\"%fs\" dur=\"%fs\" fill=\"freeze\" from=\"%d\" to=\"%d\" />\n", argv[3] * multiplier, (argv[4] - argv[3]) * multiplier, NodeY[src] - PACKET_HEIGHT/2, NodeY[dst] - PACKET_HEIGHT/2;

			}
			else if (argv[1] == "Q") {

				y = NodeQueueY[Node2Idx[argv[2]]] + (argv[4] - 1) * (PACKET_HEIGHT + INTRA_PACKET_SPACE);
				printf "\t\t<set attributeName=\"x\" to=\"%d\" begin=\"%fs\"/>\n", NodeQueueX[Node2Idx[argv[2]]] - NODE_RADIO / 2, argv[3] * multiplier;
				printf "\t\t<set attributeName=\"y\" to=\"%d\" begin=\"%fs\"/>\n", y, argv[3] * multiplier;
			}
			else if (argv[1] == "A") {

				y = NodeArrivedY[Node2Idx[argv[2]]] - (argv[4] - 1) * (PACKET_HEIGHT + INTRA_PACKET_SPACE);
				printf "\t\t<set attributeName=\"x\" to=\"%d\" begin=\"%fs\"/>\n", NodeArrivedX[Node2Idx[argv[2]]] - NODE_RADIO / 2, argv[3] * multiplier;
				printf "\t\t<set attributeName=\"y\" to=\"%d\" begin=\"%fs\"/>\n", y, argv[3] * multiplier;
			}
			
		}
		printf "\t</rect>\n";
	}
	printf "\n";

	printf "\n";
	for (i = 0; i < npackets; i++) {

		noperations = split(packets[i], operations, ";");
		color = "";
		for (j = 1; j < noperations; j++) {

			argc = split(operations[j], argv, "|");
			if (argv[1] == "T") {

				split(argv[2], a, ",");
				src = Node2Idx[a[1]];
				dst = Node2Idx[a[2]];
				if (color == "") {

					color = a[3];
					printf "\t<text x=\"%d\" y=\"%d\" font-size=\"%d\" text-anchor=\"middle\" dominant-baseline=\"middle\">%d\n", NodeX[src] + 10, NodeY[src] + 10, LABEL_FONT_SIZE, i;
				}
				
				printf "\t\t<animate attributeName=\"x\" attributeType=\"XML\" begin=\"%fs\" dur=\"%fs\" fill=\"freeze\" from=\"%d\" to=\"%d\" />\n", argv[3] * multiplier, (argv[4] - argv[3]) * multiplier, NodeX[src] + PACKET_WIDTH / 2, NodeX[dst] + PACKET_WIDTH / 2;
				printf "\t\t<animate attributeName=\"y\" attributeType=\"XML\" begin=\"%fs\" dur=\"%fs\" fill=\"freeze\" from=\"%d\" to=\"%d\" />\n", argv[3] * multiplier, (argv[4] - argv[3]) * multiplier, NodeY[src], NodeY[dst];

			}
			else if (argv[1] == "Q") {

				y = NodeQueueY[Node2Idx[argv[2]]] + (argv[4] - 1) * (PACKET_HEIGHT + INTRA_PACKET_SPACE) + PACKET_HEIGHT / 2;
				printf "\t\t<set attributeName=\"x\" to=\"%d\" begin=\"%fs\"/>\n", NodeQueueX[Node2Idx[argv[2]]], argv[3] * multiplier;
				printf "\t\t<set attributeName=\"y\" to=\"%d\" begin=\"%fs\"/>\n", y, argv[3] * multiplier;
			}
			else if (argv[1] == "A") {

				y = NodeArrivedY[Node2Idx[argv[2]]] - (argv[4] - 1) * (PACKET_HEIGHT + INTRA_PACKET_SPACE) + PACKET_HEIGHT / 2;
				printf "\t\t<set attributeName=\"x\" to=\"%d\" begin=\"%fs\"/>\n", NodeArrivedX[Node2Idx[argv[2]]], argv[3] * multiplier;
				printf "\t\t<set attributeName=\"y\" to=\"%d\" begin=\"%fs\"/>\n", y, argv[3] * multiplier;
			}
			
		}
		printf "\t</text>\n";
	}
	printf "\n";


	printf "\n";
	for (i = 0; i < nnodes; i++) {

		printf "\t<circle cx=\"%d\" cy=\"%d\" r=\"%f\" stroke=\"black\" stroke-width=\"2\" fill=\"white\"/>\n", NodeX[i], NodeY[i], NODE_RADIO;
		printf "\t<text x=\"%d\" y=\"%d\" text-anchor=\"middle\">%s</text>\n", NodeX[i], NodeY[i] + 5, NodeId[i];
	}
	printf "\n";

	printf "\n";
	nevents = split(timeEvents, a, ",");
	for (i = 2; i <= nevents; i++) {

		printf "\t<text x=\"%d\" y=\"%d\" visibility=\"hidden\">\n", CLOCK_X, CLOCK_Y;
		printf "\tt = %.2f\n", a[i] / 100.0;
		printf "\t<set attributeName=\"visibility\" to=\"visible\" begin=\"%fs\"/>\n", a[i]* multiplier;
		if (i < nevents)
			printf "\t<set attributeName=\"visibility\" to=\"hidden\" begin=\"%fs\"/>\n", a[i+1]* multiplier;
		printf "\t</text>\n";
	}
	printf "\n";

	print "</svg>\n";
}
'
