#pragma once
#include "../../Shaders/CppCommon/CudaDefines.h"
#include "../../Shaders/CppCommon/WaveFrontDataStructs.h"
#include "../../Shaders/CppCommon/WaveFrontDataStructs/LightDataBuffer.h"
#include "../../Shaders/CppCommon/WaveFrontDataStructs/AtomicBuffer.h"
#include <sutil/Matrix.h>

class PTMaterial;

CPU_ONLY void FindEmissivesWrap(
	const Vertex* a_Vertices,
	const uint32_t* a_Indices,
	bool* a_Emissives,
	const DeviceMaterial* a_Mat,
	const uint8_t a_IndexBufferSize,
	unsigned int& a_NumLights
);

CPU_ON_GPU void FindEmissives(
	const Vertex* a_Vertices, 
	const uint32_t* a_Indices,
	bool* a_Emissives,
	const DeviceMaterial* a_Mat,
	const uint8_t a_IndexBufferSize, 
	unsigned int* a_NumLights
);

CPU_ONLY void AddToLightBufferWrap(
	const Vertex* a_Vertices,
	const uint32_t* a_Indices,
	const bool* a_Emissives,
	const uint8_t a_IndexBufferSize,
	WaveFront::AtomicBuffer<WaveFront::TriangleLight>* a_Lights,
	sutil::Matrix4x4 a_TransformMat
);

CPU_ON_GPU void AddToLightBuffer(
	const Vertex* a_Vertices, 
	const uint32_t* a_Indices, 
	const bool* a_Emissives, 
	const uint8_t a_IndexBufferSize, 
	WaveFront::AtomicBuffer<WaveFront::TriangleLight>* a_Lights, 
	sutil::Matrix4x4 a_TransformMat
);

//CPU_ON_GPU void AddToLightBuffer2();