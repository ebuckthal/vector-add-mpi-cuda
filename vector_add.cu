/* Simple example demonstrating how to use MPI with CUDA
*
*  Generate some random numbers on one node.
*  Dispatch them to all nodes.
*  Compute their square root on each node's GPU.
*  Compute the average of the results using MPI.
*
*/

#include <iostream>
#include "vector_add.h"

using std::cerr;
using std::endl;


#define CUDA_CHECK(call) \
    if((call) != cudaSuccess) { \
        cudaError_t err = cudaGetLastError(); \
        cerr << "CUDA error calling \""#call"\", code is " << err << endl; \
    }

__global__ void vectorAddBin(float *Md, float *Nd, int *Pd, int width)
{
  int tid;

  tid = blockIdx.x * blockDim.x + threadIdx.x;

  while (tid < width) {
     Pd[tid] = (Md[tid] + Nd[tid] + 20) * 2;
     tid += blockDim.x * gridDim.x;
  }
  return;
}

void computeAddGPU(float *vector_a, float *vector_b, int *vector_res, int size)
{
   float *d_a;
   float *d_b;
   int *d_res;

   int blockSize = 1024;
   int gridSize = (int)ceil((float)size/blockSize);

   CUDA_CHECK(cudaMalloc(&d_a, sizeof(float)*size));
   CUDA_CHECK(cudaMalloc(&d_b, sizeof(float)*size));
   CUDA_CHECK(cudaMalloc(&d_res, sizeof(int)*size));

   CUDA_CHECK(cudaMemcpy(d_a, vector_a, sizeof(float)*size, cudaMemcpyHostToDevice));
   CUDA_CHECK(cudaMemcpy(d_b, vector_b, sizeof(float)*size, cudaMemcpyHostToDevice));

   vectorAddBin<<<gridSize, blockSize>>>(d_a, d_b, d_res, size);

   CUDA_CHECK(cudaMemcpy(vector_res, d_res, sizeof(int)*size, cudaMemcpyDeviceToHost));

   CUDA_CHECK(cudaFree(d_a));
   CUDA_CHECK(cudaFree(d_b));
   CUDA_CHECK(cudaFree(d_res));
}
