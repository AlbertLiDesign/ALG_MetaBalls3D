
#include<iostream>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <cuda_runtime.h>
#include <vector_types.h>
#include <vector_functions.h>
#include <device_launch_parameters.h>

#include <helper_cuda.h>
#include <helper_functions.h>

#include "ComputeMetaBalls.h"

extern "C" __declspec(dllexport)  bool CalcMetaBalls(cfloat3 bP, cfloat3 vS,
    int xCount, int yCount, int zCount, float s, float iso,float fu, cfloat3 * samplePoints,
    int sampleCount, size_t & resultLength);
extern "C" __declspec(dllexport)  void GetResult(cfloat3 * result);

bool CalcMetaBalls(cfloat3 bP, cfloat3 vS, int xCount, int yCount, int zCount,
    float s, float iso, float fu, cfloat3* samplePoints, int sampleCount, size_t& resultLength)
{
    bool successful = true;

    sampleLength = sampleCount;
    basePoint = Convert(bP);
    voxelSize = Convert(vS);
    gridSize = make_uint3(xCount, yCount, zCount);
    numVoxels = xCount * yCount * zCount;
    fusion = fu;
    scale = s;
    isoValue = iso;

    samplePts = new float3[sampleCount];
    for (int i = 0; i < sampleCount; i++)
    {
        samplePts[i] = Convert(samplePoints[i]);
    }

    initMetaBalls();

    int threads = 512;
    dim3 grid((numVoxels + threads - 1) / threads, 1, 1);

    // get around maximum grid size of 65535 in each dimension
    if (grid.x > 65535)
    {
        grid.y = grid.x / 32768;
        grid.x = 32768;
    }

    // calculate number of vertices need per voxel
    launch_classifyVoxel(grid, threads,
        d_voxelVerts, d_voxelOccupied, gridSize,
        numVoxels, basePoint, voxelSize, isoValue, d_samplePts, sampleLength, fusion);

    /*
    Scan occupied voxels
    after classifying voxels, we will get a lot of empty voxels.
    in order to cull them, we have to use exclusive sum scan to get a new array
    the last element in this new array plus the last element in the array before scan
    equals the number of active voxels
    */
    exclusiveSumScan(d_voxelOccupiedScan, d_voxelOccupied, numVoxels);

#pragma region Find the number of active voxels
    uint lastElement, lastScanElement;
    // only copy the last elements from two arrays on the device
    checkCudaErrors(cudaMemcpy((void*)&lastElement,
        (void*)(d_voxelOccupied + numVoxels - 1),
        sizeof(uint), cudaMemcpyDeviceToHost));
    checkCudaErrors(cudaMemcpy((void*)&lastScanElement,
        (void*)(d_voxelOccupiedScan + numVoxels - 1),
        sizeof(uint), cudaMemcpyDeviceToHost));

    // comput the number of active voxels
    num_activeVoxels = lastElement + lastScanElement;

    if (num_activeVoxels == 0)
    {
        // return if there are no full voxels
        num_resultVertices = 0;
        successful = false;
        return successful;
    }
#pragma endregion

    // Compact voxels
    // compact voxel index array
    launch_compactVoxels(grid, threads, d_compVoxelArray, d_voxelOccupied, d_voxelOccupiedScan, numVoxels);

    // Scan the number of vertices each voxel has
    // compute the number of output vertices
    exclusiveSumScan(d_voxelVertsScan, d_voxelVerts, numVoxels);

#pragma region Find the number of sum vertices
    uint lastElement2, lastScanElement2;
    checkCudaErrors(cudaMemcpy((void*)&lastElement2,
        (void*)(d_voxelVerts + numVoxels - 1),
        sizeof(uint), cudaMemcpyDeviceToHost));
    checkCudaErrors(cudaMemcpy((void*)&lastScanElement2,
        (void*)(d_voxelVertsScan + numVoxels - 1),
        sizeof(uint), cudaMemcpyDeviceToHost));
    num_resultVertices = lastElement2 + lastScanElement2;
#pragma endregion

    // free pinned memory
    cudaFreeHost(d_voxelOccupied);
    cudaFreeHost(d_voxelVerts);

#pragma region Generate isosurface

    checkCudaErrors(cudaMalloc((void**)&(d_result), num_resultVertices * sizeof(float3)));

    dim3 grid2((num_activeVoxels + threads - 1) / threads, 1, 1);

    // get around maximum grid size of 65535 in each dimension
    if (grid.x > 65535)
    {
        grid2.x /= 2;
        grid2.y *= 2;
    }

    launch_extractIsosurface(grid2, threads, d_result, d_compVoxelArray, d_voxelVertsScan,
        gridSize, basePoint, voxelSize,
        isoValue, scale, d_samplePts, sampleLength,fusion);

    resultPts = new float3[num_resultVertices];
    checkCudaErrors(cudaMemcpy(resultPts,
        d_result, num_resultVertices * sizeof(float3),
        cudaMemcpyDeviceToHost));
#pragma endregion

    cleanup();

    resultLength = (size_t)num_resultVertices;

    return successful;
}
void GetResult(cfloat3* results)
{
    for (int i = 0; i < num_resultVertices; i++)
    {
        results[i] = Convert(resultPts[i]);
    }
    delete[] resultPts;
}


void initMetaBalls()
{
    // allocate device memory
    uint memSize = sizeof(uint) * numVoxels;
    checkCudaErrors(cudaMalloc((void**)&d_voxelVertsScan, memSize));
    checkCudaErrors(cudaMalloc((void**)&d_voxelOccupiedScan, memSize));
    checkCudaErrors(cudaMalloc((void**)&d_compVoxelArray, memSize));
    // allocate sample points and copy them to device
    checkCudaErrors(cudaMalloc((void**)&d_samplePts, sampleLength * sizeof(float3)));
    cudaMemcpy(d_samplePts, samplePts, sampleLength * sizeof(float3), cudaMemcpyHostToDevice);

    // allocate pinned memory
    cudaError_t status = cudaMallocHost((void**)&d_voxelOccupied, memSize);
    if (status != cudaSuccess)
        printf("Error allocating pinned host memory\n");

    cudaError_t status2 = cudaMallocHost((void**)&d_voxelVerts, memSize);
    if (status2 != cudaSuccess)
        printf("Error allocating pinned host memory\n");
}
void cleanup()
{
    checkCudaErrors(cudaFree(d_result));

    checkCudaErrors(cudaFree(d_edgeTable));
    checkCudaErrors(cudaFree(d_triTable));
    checkCudaErrors(cudaFree(d_numVertsTable));

    checkCudaErrors(cudaFree(d_voxelVertsScan));
    checkCudaErrors(cudaFree(d_voxelOccupiedScan));
    checkCudaErrors(cudaFree(d_compVoxelArray));

    checkCudaErrors(cudaFree(d_samplePts));
    delete[] samplePts;
}


