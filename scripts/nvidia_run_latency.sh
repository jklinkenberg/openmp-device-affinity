#!/bin/bash

RES_DIR="$(pwd)/results"
mkdir -p ${RES_DIR}

# OpenMP target based
export USE_CUDA=0
export GPU_ARCH=sm_60
sbatch --partition=c16g --gres=gpu:pascal:2 --account=supp0001 --export=GPU_ARCH,USE_CUDA --output=${RES_DIR}/results_lat_c16g.txt nvidia_run_latency.sbatch
export GPU_ARCH=sm_70
sbatch --partition=c16g --gres=gpu:2 --account=supp0001 --export=GPU_ARCH,USE_CUDA --output=${RES_DIR}/results_lat_c18g.txt nvidia_run_latency.sbatch

# CUDA based
export USE_CUDA=1
export GPU_ARCH=sm_60
sbatch --partition=c16g --gres=gpu:pascal:2 --account=supp0001 --export=GPU_ARCH,USE_CUDA --output=${RES_DIR}/results_lat_c16g-cuda.txt nvidia_run_latency.sbatch
export GPU_ARCH=sm_70
sbatch --partition=c16g --gres=gpu:2 --account=supp0001 --export=GPU_ARCH,USE_CUDA --output=${RES_DIR}/results_lat_c18g-cuda.txt nvidia_run_latency.sbatch