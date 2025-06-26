# FITPATH: Efficient Multipath Selection for IoT Video Transmission


FITPATH is an efficient and adaptable path selection scheme for multipath routing that can accommodate scenarios where multiple video sources can simultaneously transmit heterogeneous video flows, i.e., flows of different bitrates. FITPATH uses a novel heuristic-based iterative optimization approach that estimates the conditions of the underlying network in real time while accounting for the different bitrate requirements of the application video flows.

# Instructions
The root directory contains the source code and the directory data. The directory data must contain all the data of the project, like scenarios, instances, and other input and output of the project. 

* `estimates`: Contains the output files, with the estimates returned in each simulation.
* `etx`: Contains the input files of the network topology, with the and quality of the links (etx).
* `instances`: Contains the instances generated with the different sources and destinations.
* `routes`: Contains output files with routes used as input to ns-3 to validate performance.
* `scenarios`: Contains the scenarios generated with the different number and positioning of the network nodes.

# Building

Clone this repository on your local machine and run:

    make fitpath

# Running

Once MAPE binaries has been linked, simply run the `./fitpath` `<topology>` `<inst_dir>` found in the root of the project directory.

Examples:

    $ ./fitpath ext/etxGrid56 instance/instGrid3
 
# Contacts
For further information contact Fabiano Bhering at fabianobhering@cefetmg.br.
