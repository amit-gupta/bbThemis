### Efficient Bulk Transfer over Lustre (EBT)

1. Plan for benchmark runs on Frontera/LS6 (right after maintenance)
	To run the benchmarks with/without EBT to compare I/O time consumption (also collecting app time-to-solution) and the CPU utilization of Lustre servers (MDS and OSS).

2. Opportunities for EBT
	2.1, Write cache, then merge and transfer at the end of the job to Lustre. 
		 Bulk metadata update without massive concurrency.
		 OST-aware output to balance writing targets
	2.2, Global Read cache
		 A process queries a file before reading from Lustre, content is supplied via messages from peers nodes instead of Lustre
		 Need to maintain a registry of file names and location

3. Communication backend
	3.1 UCX (https://github.com/openucx/ucx)
	3.2 MPI

4. Experiment platforms
	3.1 Frontera Lustre. Zhao to figure out the maintenance schedule and get experiment ready before maintenance.
	3.2 LS6 BeeGFS
	3.3 Lustre on AWS (https://docs.aws.amazon.com/fsx/latest/LustreGuide/what-is.html). Ian or Zhao to ask TACC about the AWS account.

SC'23 Deadline April 6th, 2023
