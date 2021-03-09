#pragma once
#include "../Shaders/CppCommon/CudaDefines.h"
#include "../Shaders/CppCommon/WaveFrontDataStructs.h"

#include <cuda_runtime.h>


//Some defines to make the functions less scary and more readable

using namespace WaveFront;






/*
 * Called at start of frame.
 * Generates the camera rays.
 * Synchronizes with device at the end of the function.
 */
CPU_ONLY void GeneratePrimaryRays(const PrimRayGenLaunchParameters& a_PrimRayGenParams);

/*
 * 
 */
CPU_ONLY void GenerateMotionVectors();

/*
 * Called each wave after resolving a RayBatch.
 * Shade the intersection points.
 * This does direct, indirect and specular shading.
 * This fills the ShadowRayBatch with potential contributions per pixel and a ray definition.
 * Synchronizes with device at the end of the function.
 */
CPU_ONLY void Shade(const ShadingLaunchParameters& a_ShadingParams);

/*
 * Called at the end of the frame.
 * Apply de-noising, up scaling and post-processing effects.
 */
CPU_ONLY void PostProcess(const PostProcessLaunchParameters& a_PostProcessParams);



//The below functions are only called internally from the GPU_ONLY within the above defined functions.

//Generate some rays based on the thread index.
CPU_ON_GPU void GeneratePrimaryRay(
    int a_NumRays, 
    IntersectionRayBatch* const a_Buffer, 
    float3 a_U, 
    float3 a_V, 
    float3 a_W, 
    float3 a_Eye, 
    int2 a_Dimensions);

//Called during shading

/*
 * Called at the start of the Shade function with a number of thread-blocks.
 * Calculates direct shading as potential light contribution.
 * Defines a shadow ray that is used to validate the potential light contribution.
 */
CPU_ON_GPU void ShadeDirect(
    const uint3 a_ResolutionAndDepth, 
    const IntersectionRayBatch* const a_CurrentRays, 
    const IntersectionBuffer* const a_CurrentIntersections, 
    ShadowRayBatch* const a_ShadowRays, 
    const LightBuffer* const a_Lights, 
    CDF* const a_CDF /*const CDF* a_CDF*/);

/*
 *
 */
CPU_ON_GPU void ShadeSpecular();

/*
 *
 */
CPU_ON_GPU void ShadeIndirect(
    const uint3 a_ResolutionAndDepth, 
    const IntersectionRayBatch* const a_PreviousRays, 
    const IntersectionBuffer* const a_Intersections, 
    IntersectionRayBatch* const a_Output);


CPU_ON_GPU void DEBUGShadePrimIntersections(
    const uint3 a_ResolutionAndDepth,
    const IntersectionRayBatch* const a_PrimaryRays,
    const IntersectionBuffer* const a_PrimaryIntersections,
    ResultBuffer* const a_Output);

//Called during post-processing.

/*
 *
 */
CPU_ON_GPU void Denoise();

/*
 *
 */
CPU_ON_GPU void MergeLightChannels(
    const uint2 a_Resolution, 
    const ResultBuffer* const a_Input, 
    PixelBuffer* const a_Output);

/*
 *
 */
CPU_ON_GPU void DLSS();

/*
 *
 */
CPU_ON_GPU void PostProcessingEffects();

//Temporary step till post-processing is in place.
CPU_ON_GPU void WriteToOutput(
    const uint2 a_Resolution, 
    const PixelBuffer* const a_Input, 
    uchar4* a_Output);



//Helper functions.

GPU_ONLY INLINE unsigned int WangHash(unsigned int a_S);

GPU_ONLY INLINE unsigned int RandomInt(unsigned int& a_S);

GPU_ONLY INLINE float RandomFloat(unsigned int& a_S);



//Data buffer helper functions.

//Reset ray batch.

CPU_ONLY void ResetRayBatch(
    IntersectionRayBatch* const a_RayBatchDevPtr,
    unsigned int a_NumPixels,
    unsigned int a_RaysPerPixel);

CPU_ON_GPU void ResetRayBatchMembers(
    IntersectionRayBatch* const a_RayBatch, 
    unsigned int a_NumPixels, 
    unsigned int a_RaysPerPixel);

CPU_ON_GPU void ResetRayBatchData(IntersectionRayBatch* const a_RayBatch);

//Reset shadow ray batch.

CPU_ONLY void ResetShadowRayBatch(
    ShadowRayBatch* a_ShadowRayBatchDevPtr,
    unsigned int a_MaxDepth,
    unsigned int a_NumPixels,
    unsigned int a_RaysPerPixel);

CPU_ON_GPU void ResetShadowRayBatchMembers(
    ShadowRayBatch* const a_ShadowRayBatch, 
    unsigned int a_MaxDepth, 
    unsigned int a_NumPixels, 
    unsigned int a_RaysPerPixel);

CPU_ON_GPU void ResetShadowRayBatchData(ShadowRayBatch* const a_ShadowRayBatch);

//Reset PixelBuffer.

CPU_ONLY void ResetPixelBuffer(
    PixelBuffer* a_PixelBufferDevPtr,
    unsigned int a_NumPixels,
    unsigned int a_ChannelsPerPixel);

CPU_ON_GPU void ResetPixelBufferMembers(
    PixelBuffer* const a_PixelBuffer,
    unsigned int a_NumPixels,
    unsigned int a_ChannelsPerPixel);

CPU_ON_GPU void ResetPixelBufferData(PixelBuffer* const a_PixelBuffer);