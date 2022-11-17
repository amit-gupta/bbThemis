#!/bin/bash
#SBATCH -J vpic_224_4_1
#SBATCH -o vpic_224_4_1.%j 
#SBATCH -N 4
#SBATCH -n 224
#SBATCH -p debug
#SBATCH -t 01:00:00
#SBATCH -A A-ccsc

export OMP_NUM_THREADS=1

export vpicexe=/scratch1/06058/iwang/benchmarks/vpic/clx/224/test_224.ft_clx_intel-18.0.2_impi-18.0.2_-xCORE-AVX512
export NP=224
export NPPNODE=56

export DXT_ENABLE_IO_TRACE=8
export LD_PRELOAD=/scratch1/06058/iwang/benchmarks/montage/darshan-3.2.1/lib//libdarshan.so

#module purge
#module load intel/18.0.2
#module load impi/18.0.2

mkdir ${SLURM_JOBID}
cd ${SLURM_JOBID}

echo $DXT_ENABLE_IO_TRACE
date
DXT_ENABLE_IO_TRACE=8 time -p ibrun tacc_affinity ${vpicexe} -tpp=1

cp ../vpic_job_224_4_1_darshan.sh .
