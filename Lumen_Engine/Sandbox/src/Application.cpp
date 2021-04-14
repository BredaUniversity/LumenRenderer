#include <Lumen.h>
#include <string>
#include <filesystem>
#include <algorithm>

#include "OutputLayer.h"

#include "imgui/imgui.h"
#include "ModelLoading/Node.h"

#include "GLFW/include/GLFW/glfw3.h"
#include "Lumen/ModelLoading/SceneManager.h"

#ifdef WAVEFRONT
#include "../../LumenPT/src/Framework/WaveFrontRenderer.h"
#else
#include "../../LumenPT/src/Framework/OptiXRenderer.h"
#endif


//#include "imgui/imgui.h"

namespace Lumen {
    class WindowsWindow;
}

namespace LumenPTConsts
{
	const static std::string ShaderPath = "whatever";
}

class ExampleLayer : public Lumen::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		
	}
	
	void OnUpdate() override
	{
		if(Lumen::Input::IsKeyPressed(LMN_KEY_TAB))
			LMN_INFO("Tab  key is pressed");
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Test");
		ImGui::Text("Hello Lumen Renderer Engine, We gon' path trace the hell outta you.");
		ImGui::End();
	}

	void OnEvent(Lumen::Event& event) override
	{

		if(event.GetEventType() == Lumen::EventType::KeyPressed)
		{
			Lumen::KeyPressedEvent& e = static_cast<Lumen::KeyPressedEvent&>(event);
			//LMN_TRACE("{0}", static_cast<char>(e.GetKeyCode()));
		}
	}
};


class Sandbox : public Lumen::LumenApp
{
public:
	Sandbox()
	{
		glfwMakeContextCurrent(reinterpret_cast<GLFWwindow*>(GetWindow().GetNativeWindow()));
		//PushOverlay(new Lumen::ImGuiLayer());
		

		OutputLayer* m_ContextLayer = new OutputLayer;
		PushLayer(m_ContextLayer);


		//temporary stuff to avoid absolute paths to gltf cube
		std::filesystem::path p = std::filesystem::current_path();
		std::string p_string{ p.string() };
		std::replace(p_string.begin(), p_string.end(), '\\', '/');
		//p_string.append("/Sandbox/assets/models/Lantern.gltf");

		std::filesystem::path p2 = std::filesystem::current_path();
		std::string p_string2{ p2.string() };
		std::replace(p_string2.begin(), p_string2.end(), '\\', '/');

		const std::string meshPath = p_string.append("/Sandbox/assets/models/Sponza/");
		const std::string meshPath2 = p_string2.append("/Sandbox/assets/models/EmissiveSphere/");
		//Base path for meshes.

		//Mesh name
		const std::string meshName = "Sponza.gltf";
		const std::string meshName2 = "EmissiveSphere.gltf";

		//p_string.append("/Sandbox/assets/models/Sponza/Sponza.gltf");
		LMN_TRACE(p_string);

	    m_SceneManager->SetPipeline(*m_ContextLayer->GetPipeline());
		auto res = m_SceneManager->LoadGLTF(meshName, meshPath);
		auto res2 = m_SceneManager->LoadGLTF(meshName2, meshPath2);

		auto lumenPT = m_ContextLayer->GetPipeline();

		LumenRenderer::SceneData scData = {};
		
		lumenPT->m_Scene = lumenPT->CreateScene(scData);
		
		//Loop over the nodes in the scene, and add their meshes if they have one.

		float xOffset = -900.f;

		for(int i = 0; i < 30; ++i)
		{
			for (auto& node : res2->m_NodePool)
			{
				auto meshId = node->m_MeshID;
				//if (meshId >= 0)
				{
					auto mesh = lumenPT->m_Scene->AddMesh();
					mesh->SetMesh(res2->m_MeshPool[meshId]);
					//mesh->m_Transform.CopyTransform(*node->m_LocalTransform);
					float p = i;
					mesh->m_Transform.SetPosition(glm::vec3(xOffset, 100.f + (p*p), 0.f));
					mesh->m_Transform.SetScale(glm::vec3(2.0f * (static_cast<float>(i * 2) / 4.f)));
				}
			}

			xOffset += 50;
		}

		for(auto& node: res->m_NodePool)
		{
			auto meshId = node->m_MeshID;
			if(meshId >= 0)
			{
				auto mesh = lumenPT->m_Scene->AddMesh();
				mesh->SetMesh(res->m_MeshPool[meshId]);
				//mesh->m_Transform.CopyTransform(*node->m_LocalTransform);
				mesh->m_Transform.SetPosition(glm::vec3(0.f, 0.f, 15.0f));
				mesh->m_Transform.SetScale(glm::vec3(1.0f));
			}
		}

		//for (auto& node : res2->m_NodePool)
		//{
		//	auto meshId = node->m_MeshID;
		//	//if (meshId >= 0)
		//	{
		//		auto mesh = lumenPT->m_Scene->AddMesh();
		//		mesh->SetMesh(res2->m_MeshPool[meshId]);
		//		//mesh->m_Transform.CopyTransform(*node->m_LocalTransform);
		//		mesh->m_Transform.SetPosition(glm::vec3(100.f, 500.f, 0.0f));
        //		mesh->m_Transform.SetScale(glm::vec3(3000.0f));
		//	}
		//}

		//lumenPT->m_Scene = lumenPT->CreateScene(scData);
		//auto mesh = lumenPT->m_Scene->AddMesh();
		//mesh->SetMesh(res->m_MeshPool[0]);

		//mesh->m_Transform.SetPosition(glm::vec3(0.f, 0.f, 15.0f));
		//mesh->m_Transform.SetScale(glm::vec3(1.0f));

		//auto mesh2 = lumenPT->m_Scene->AddMesh();
		//mesh2->SetMesh(res2->m_MeshPool[0]);
		//mesh2->m_Transform.SetPosition(glm::vec3(0.f, 30.f, 15.0f));
		//mesh2->m_Transform.SetScale(glm::vec3(1.0f));
		//
		
		//meshLight->m_Transform.SetPosition(glm::vec3(0.f, 0.f, 15.0f));
		//meshLight->m_Transform.SetScale(glm::vec3(1.0f));

		//std::string vndbFilePath = { p.string() };
		////vndbFilePath.append("/Sandbox/assets/volume/Sphere.vndb");
		//vndbFilePath.append("/Sandbox/assets/volume/bunny.vdb");
		//auto volumeRes = m_SceneManager->m_VolumeManager.LoadVDB(vndbFilePath);

		//auto volume = lumenPT->m_Scene->AddVolume();
		//volume->SetVolume(volumeRes->m_Volume);
	}

	~Sandbox()
	{
		
	}
};

Lumen::LumenApp* Lumen::CreateApplication()
{
	return new Sandbox();
}
