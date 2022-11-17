#!/bin/bash
#SBATCH -A A-ccsc
#SBATCH --time=01:00:00
#SBATCH -o milc-small.%j.out
#SBATCH -p debug
#SBATCH -N 3
#SBATCH --ntasks-per-node 48

ml purge
ml intel/19 impi/19 

#ml purge
#ml use /scratch/projects/compilers/modulefiles 
#ml intel/19.0.2 impi/19.0.2
#ml use /scratch/projects/tacc/spp/rtevans/builder/src/lmod/modulefiles
#ml skylake-novec milc
#ml

. milc_in.sh
total_nodes=$SLURM_JOB_NUM_NODES             
cores_per_node=$SLURM_NTASKS_PER_NODE
numa_per_node=2
threads_per_rank=1

echo "$SLURM_JOB_NODELIST" > node_list.$SLURM_JOBID
echo "NODES in allocation is $total_nodes"

# Parse command line parameters
debug="false"
while [ "$#" -ge 1 ] ; do
    case "$1" in
	"--build" )           build_lattice=true; shift 1;;
	"--debug" )           debug="true"; shift 1;;
	"--cores" | "-c" )    cores_per_node=$2; shift 2;;
      "--threads" | "-t" )  threads_per_rank=$2; shift 2;;
	*)                    break;;
    esac
done

#----- END USER DEFINED PARAMETERS -----#

# Set problem size
nx=18
ny=18
nz=18
nt=36

N=$(( $total_nodes*$cores_per_node/$threads_per_rank ))  # total ranks
S=$(( $N/$total_nodes/$numa_per_node ))                 # ranks per socket
#source /scratch/hpc_tools/spp_ibrun/sourceme.sh
S="ibrun "

# sanity check for parameters
if [ $(( $N*$threads_per_rank )) -gt $(( total_nodes*$cores_per_node )) ]; then
    echo "$0: ERROR, number of threads exceeds total concurrency, aborting"
    exit;
else
    echo "$0: Using $N MPI rank(s), $S rank(s) per NUMA domain, $threads_per_rank thread(s) per rank"
fi

dimension=${nx}x${ny}x${nz}x${nt}
checklat=$dimension.chklat
if [ "$build_lattice" == "true" ]; then
    warms=40
    trajecs=2
    save_cmd="save_serial $checklat"
    reload_cmd="continue"
    echo "$0: Building lattice $dimension on $total_nodes nodes"
else
    warms=0
    trajecs=2
    save_cmd="forget"
    reload_cmd="reload_parallel $checklat"
    echo "$0: Running lattice $dimension on $total_nodes nodes"
fi

# Run MILC
echo -n "$0: Begin time: "; date
echo "UNIX_TIME_START=`date +%s`"
# Write output to a file that we can see during execution
#  run_milc
run_milc > milc_${dimension}_on_${total_nodes}_${SLURM_NTASKS_PER_NODE}_${threads_per_rank}.$SLURM_JOBID
echo -n "$0: End time: "; date
echo "UNIX_TIME_END=`date +%s`"
