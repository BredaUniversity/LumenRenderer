//
// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <Optix/optix.h>

//#include "main.h"
#include <cuda/helpers.h>

#include <sutil/vec_math.h>

#include "CppCommon/LaunchParameters.h"

extern "C" {
__constant__ LaunchParameters params1;
}


static __forceinline__ __device__ void setPayload( float3 p )
{
    optixSetPayload_0( float_as_int( p.x ) );
    optixSetPayload_1( float_as_int( p.y ) );
    optixSetPayload_2( float_as_int( p.z ) );
}


static __forceinline__ __device__ void computeRay( uint3 idx, uint3 dim, float3& origin, float3& direction )
{
    const float3 U = params1.cam_u;
    const float3 V = params1.cam_v;
    const float3 W = params1.cam_w;
    const float2 d = 2.0f * make_float2(
            static_cast<float>( idx.x ) / static_cast<float>( dim.x ),
            static_cast<float>( idx.y ) / static_cast<float>( dim.y )
            ) - 1.0f;

    origin    = params1.cam_eye;
    direction = normalize( d.x * U + d.y * V + W );
}


extern "C" __global__ void __raygen__rg()
{
    // Lookup our location within the launch grid
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    // Map our launch idx to a screen location and create a ray from the camera
    // location through the screen
    float3 ray_origin, ray_direction;
    computeRay( idx, dim, ray_origin, ray_direction );

    // Trace the ray against our scene hierarchy
    unsigned int p0, p1, p2;
    optixTrace(
            params1.handle,
            ray_origin,
            ray_direction,
            0.0f,                // Min intersection distance
            1e16f,               // Max intersection distance
            0.0f,                // rayTime -- used for motion blur
            OptixVisibilityMask( 255 ), // Specify always visible
            OPTIX_RAY_FLAG_NONE,
            0,                   // SBT offset   -- See SBT discussion
            1,                   // SBT stride   -- See SBT discussion
            0,                   // missSBTIndex -- See SBT discussion
            p0, p1, p2 );
    float3 result;
    result.x = int_as_float( p0 );
    result.y = int_as_float( p1 );
    result.z = int_as_float( p2 );

    // Record results in our output raster
    params1.frame_buffer[idx.y * params1.width + idx.x] = make_color( result );
}


extern "C" __global__ void __miss__ms()
{
    MissData* miss_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    setPayload(  miss_data->bg_color );
}


extern "C" __global__ void __closesthit__ch()
{
    // When built-in triangle intersection is used, a number of fundamental
    // attributes are provided by the OptiX API, indlucing barycentric coordinates.
    const float2 barycentrics = optixGetTriangleBarycentrics();
    auto sbtData = reinterpret_cast<HitData*>(optixGetSbtDataPointer());

    auto col = tex2D<float4>(sbtData->tex, barycentrics.x, 1 - barycentrics.y);

    auto c = make_float3(col.x, col.y, col.z);

    setPayload( c * c );
}
