#!/bin/bash
#SBATCH -J test_gmx
#SBATCH -o test.o%j
#SBATCH -e test.e%j
#SBATCH -p debug
#SBATCH -N 2
#SBATCH -n 112
#SBATCH -t 1:00:00

module reset
module load intel/19.1.1
module load impi/19.0.9
module load gromacs/2020.1

#ibrun mdrun_mpi -noconfout -s ../../benchPEP.tpr -deffnm md -g md.log -nsteps 1000

#export DXT_ENABLE_IO_TRACE=8
#export LD_PRELOAD=/scratch1/06058/iwang/benchmarks/montage/darshan-3.2.1/lib//libdarshan.so


# with writting a big conformation file at the end of the simulation
ibrun strace_mpi mdrun_mpi -s ./benchPEP.tpr -deffnm md -g md.log -nsteps 1000


