# Oct 14th, 2021

### I/O consistency and file system

- We want to study the impact of consistency on application and file system performance.

- We know the I/O consistency for some well-known applications, e.g., WRF, NAMD, SPECFEM3D, …
There are also applications we have no idea of, neither do the users or code authors.
So we may gather the consistency info from two sources: known applications and users’ supply

- If the consistency models do lead to performance differences, we may propose a file system that supports multiple consistency models at the same time.
Further, they system may be able to adjust the consistency model on the run or with offline analysis

### Quantitative I/O resource management

- In the proposal, we said we will investigate the numerical I/O resource provisioning mechanism assuming applications are submitted with the I/O requirement, e.g., in throughput or IOPS.
Now I feel that, neither do we nor the users, or even the code authors know clearly what the I/O requirements are. 
Such a numerical I/O resource provisioning tool will be more meaningful to system administrators, who monitors the file system status from time to time. So they know clearly the risk of running the next application in the queue.
Such integration can also be done with system schedulers, e.g., SLURM, so that SLURM knows the risk of putting another job into execution and can make decisions on the predicted outcome. This is assuming the application performance can be inferred from I/O performance.

- To do this, we need to determine the mapping between I/O requests (with consistency constraints) to actual time consumption. We need to figure out the total available I/O cycles on a burst buffer system and assign the cycles to applications. We need a complex model to translate the diverse I/O workloads to a few numerical values.


### Dan's comments:

- assume we know the consistency model followed by an application, and determine how we use that information in a system

- think about how to determine the consistency model of an application - developer provides, user projects, system tests

### Bill's comments:

- One thing that is hard to capture at the application level is the impact of strong consistency models on file system stability. In short, the complex implementations needed to provide performance when strong consistency is required introduce many ways for the file system to fail - for example, distributed state must be managed. Systems with weaker consistency models don’t have to twist themselves into pretzels to deliver performance.  We should at least keep this in mind as we look at the impact of consistency on the applications and file system.