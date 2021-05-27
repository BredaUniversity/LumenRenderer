#pragma once
#if defined(WAVEFRONT)
#include "OptixWrapper.h"
#include "CudaGLTexture.h"
#include "MemoryBuffer.h"
#include "MotionVectors.h"
#include "../Shaders/CppCommon/WaveFrontDataStructs.h"
#include "PTServiceLocator.h"
#include "DX12 Wrapper/INRIWrapper.h"
#include "DX12 Wrapper/IDLSSWrapper.h"

#include "Renderer/LumenRenderer.h"

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>

class SceneDataTable;

class FrameSnapshot;
namespace WaveFront
{
    struct WaveFrontSettings
    {

        std::filesystem::path m_ShadersFilePathSolids;

        std::filesystem::path m_ShadersFilePathVolumetrics;

        //The maximum path length in the scene.
        std::uint32_t depth;

        //The resolution to render at.
        uint2 renderResolution;

        //The resolution to output at (up-scaling).
        uint2 outputResolution;

        //The minimum distance a ray has to travel before intersecting a surface.
        float minIntersectionT;

        //The maximum distance a ray can travel before terminating.
        float maxIntersectionT;

        bool blendOutput;
    };

    class WaveFrontRenderer : public LumenRenderer
    {
        //Overridden functionality.
    public:
        void StartRendering() override;
        void PerformDeferredOperations() override;

        std::unique_ptr<Lumen::ILumenPrimitive> CreatePrimitive(PrimitiveData& a_PrimitiveData) override;
        std::shared_ptr<Lumen::ILumenMesh> CreateMesh(std::vector<std::shared_ptr<Lumen::ILumenPrimitive>>& a_Primitives) override;
        std::shared_ptr<Lumen::ILumenTexture> CreateTexture(void* a_PixelData, uint32_t a_Width, uint32_t a_Height) override;
        std::shared_ptr<Lumen::ILumenMaterial> CreateMaterial(const MaterialData& a_MaterialData) override;
        std::shared_ptr<Lumen::ILumenVolume> CreateVolume(const std::string& a_FilePath) override;
        std::shared_ptr<Lumen::ILumenScene> CreateScene(SceneData a_SceneData) override;

        //Public functionality
    public:
        WaveFrontRenderer();

        ~WaveFrontRenderer();

        /*
         * Render.
         */
        unsigned GetOutputTexture() override;

        /*
         * Initialize the wavefront pipeline.
         * This sets up all buffers required by CUDA.
         */
        void Init(const WaveFrontSettings& a_Settings);

        void BeginSnapshot() override;

        std::unique_ptr<FrameSnapshot> EndSnapshot() override;

        void SetRenderResolution(glm::uvec2 a_NewResolution) override;
        void SetOutputResolution(glm::uvec2 a_NewResolution) override;
        glm::uvec2 GetRenderResolution() override;
        glm::uvec2 GetOutputResolution() override;

        /*
         * Set the append mode.
         * When true, final output is blended and not overwritten to build a higher res image over time.
         * When false, output is overwritten every frame.
         */
        void SetBlendMode(bool a_Blend) override;

        /*
         * Get the append mode.
         * When true, output is blended and not overwritten.
         */
        bool GetBlendMode() const override;
    	
    private:

        void TraceFrame();

        std::unique_ptr<MemoryBuffer> InterleaveVertexData(const PrimitiveData& a_MeshData) const;

        void ResizeBuffers();

        void SetOutputResolutionInternal(glm::uvec2 a_NewResolution);
        void WaitForDeferredCalls(); // Stalls the thread until all calls that are deferred to other treads are performed

        //OpenGL buffer to write output to.
        std::unique_ptr<CudaGLTexture> m_OutputBuffer;

        // Intermediate buffer to store path tracing results without interfering with OpenGL rendering thread
        MemoryBuffer m_IntermediateOutputBuffer;

        //The surface data per pixel. 0 and 1 are used for the current and previous frame. 2 is used for any other depth.
        MemoryBuffer m_SurfaceData[3];

        //The volumetric data per pixel.
        MemoryBuffer m_VolumetricData[1];

        //Intersection points passed to Optix.
        MemoryBuffer m_IntersectionData;

		//Volumetric intersection data passed to Optix
		MemoryBuffer m_VolumetricIntersectionData;

        //Buffer containing intersection rays.
        MemoryBuffer m_Rays;

        //Buffer containing shadow rays.
        MemoryBuffer m_ShadowRays;

        //Buffer containing volumetric shadow rays.
        MemoryBuffer m_VolumetricShadowRays;

        //Buffer used for output of separate channels of light.
        MemoryBuffer m_PixelBufferSeparate;

        //Buffer used to combine light channels after denoising.
        MemoryBuffer m_PixelBufferCombined;

        //Triangle lights.
        MemoryBuffer m_TriangleLights;

    	//Buffer containing motion vectors
        MotionVectors m_MotionVectors;

        //Optix system
        std::unique_ptr<OptixWrapper> m_OptixSystem;

        //NRI Wrapper
        std::unique_ptr<INRIWrapper> m_NRD;

        //DLSS Wrapper
        std::unique_ptr<IDLSSWrapper> m_DLSS;

        //ReSTIR
        std::unique_ptr<ReSTIR> m_ReSTIR;

        //Variables and settings.
    private:
        WaveFrontSettings m_Settings; // Settings to use while rendering
        WaveFrontSettings m_IntermediateSettings; // Settings used to make changes from other threads without affecting the rendering process
        std::mutex m_SettingsUpdateMutex;

        std::queue<std::function<void()>> m_DeferredOpenGLCalls;
        std::condition_variable m_OGLCallCondition;

        //Blend counter when blending is enabled.
        unsigned m_BlendCounter;

        //Index of the frame, used to swap buffers.
        std::uint32_t m_FrameIndex;

        //The CUDA instance context stuff
        CUcontext m_CUDAContext;

        //The server locator instance.
        PTServiceLocator m_ServiceLocator;

        std::mutex m_OutputBufferMutex;
        std::thread m_PathTracingThread;
        std::condition_variable m_OutputCondition;
        class GLFWwindow* m_GLContext;
        bool m_StopRendering;

        GLuint m_OutputTexture;

        // The Frame Snapshot is used to define what to record when the output layer requests that
        // See TraceFrame() ##ToolsBookmark for example
        std::unique_ptr<FrameSnapshot> m_FrameSnapshot;
        bool m_SnapshotReady;
        bool m_StartSnapshot;

    };
}
#endif