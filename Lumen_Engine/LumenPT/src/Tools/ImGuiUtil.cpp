
#include "ImGuiUtil.h"

#include "../Framework/CudaGLTexture.h"

#include "../imgui/imgui.h"

namespace ImGuiUtil
{
    void DisplayImage(GLuint a_GLTexture, glm::ivec2 a_Size, glm::vec2 a_UV1, glm::vec2 a_UV2)
    {
        ImVec2 uv1(a_UV1.x, a_UV1.y);
        ImVec2 uv2(a_UV2.x, a_UV2.y);
        ImVec2 size(a_Size.x, a_Size.y);
        ImGui::Image(reinterpret_cast<void*>(a_GLTexture), size, uv1, uv2);
    }

    void DisplayImage(CudaGLTexture& a_CudaGLTexture, glm::vec2 a_UV1, glm::vec2 a_UV2)
    {
        DisplayImage(a_CudaGLTexture.GetTexture(), a_CudaGLTexture.GetSize(), a_UV1, a_UV2);
    }
}
