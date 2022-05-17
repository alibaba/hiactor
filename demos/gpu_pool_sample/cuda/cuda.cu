#include <device_launch_parameters.h>
#include <cuda_runtime.h>
#include "cuda.h"

//Kernel
__global__ void d_vec_add(int *d_a, int *d_b, int *d_c,int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        d_c[i] = d_a[i] + d_b[i];
}

extern "C"
void* d_alloc_space(unsigned num_bytes) {
    void *ret;
    cudaMalloc(&ret, num_bytes);
    return ret;
}

extern "C"
void d_free_space(void *ptr) {
    cudaFree(ptr);
}

extern "C" 
void h_vec_add(stream_handle strm_hdl, int *a, int *b, int *c, int *d_a, int *d_b, int *d_c, unsigned n) {
    cudaMemcpyAsync(d_a, a, sizeof(int) * n, cudaMemcpyHostToDevice, static_cast<cudaStream_t>(strm_hdl));
    cudaMemcpyAsync(d_b, b, sizeof(int) * n, cudaMemcpyHostToDevice, static_cast<cudaStream_t>(strm_hdl));

    dim3 DimGrid(n / BX + 1, 1, 1);
    dim3 DimBlock(BX, 1, 1);
    d_vec_add<<<DimGrid, DimBlock, 0, static_cast<cudaStream_t>(strm_hdl)>>>(d_a, d_b, d_c, n);

    cudaMemcpyAsync(c, d_c, sizeof(int) * n, cudaMemcpyDeviceToHost, static_cast<cudaStream_t>(strm_hdl));
}
