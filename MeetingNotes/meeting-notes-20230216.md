### Efficient Bulk Transfer over Lustre (EBT)

1. Plan for benchmark runs on Frontera/LS6 (right after maintenance)

	To run the benchmarks with/without EBT to compare I/O time consumption (also collecting app time-to-solution) and the CPU utilization of Lustre servers (MDS and OSS).

	02022023 - Simple Read tests on LS6 $WORK (Lustre)
	- https://github.com/bbThemis/bbThemis/blob/master/LustreBulk/README.md

	Add the CM1 application: https://www2.mmm.ucar.edu/people/bryan/cm1/

2. Opportunities for EBT

	2.1, Write cache, then merge and transfer at the end of the job to Lustre. 
		 Bulk metadata update without massive concurrency.
		 OST-aware output to balance writing targets

	2.2, Global Read cache
		 A process queries a file before reading from Lustre, content is supplied via messages from peers nodes instead of Lustre
		 Need to maintain a registry of file names and location

	2.3, EBT staging tools ready (https://github.com/bbThemis/bbThemis/blob/master/LustreBulk/README.md). 

3. Communication backend

	3.1 UCX (https://github.com/openucx/ucx)
		Yuhong protopyted a server-client program, now in scalability test then ready to transition to ThemisIO.


4. Experiment platforms

	3.1 Frontera Lustre. Zhao to figure out the maintenance schedule and get experiment ready before maintenance. -- Zhao to prioritize

	3.2 LS6 BeeGFS

	3.3 Lustre on AWS (https://docs.aws.amazon.com/fsx/latest/LustreGuide/what-is.html). Ian or Zhao to ask TACC about the AWS account.

	3.4 Figuring out how to launch a Lustre on AWS EC2. 

SC'23 Deadline April 6th, 2023
