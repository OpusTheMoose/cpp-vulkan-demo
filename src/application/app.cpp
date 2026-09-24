module;
#include "SDL3/SDL_video.h"
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <print>
#include <stop_token>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <print>
#include <thread>

#include <imgui.h>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

export module app;
import window;
import event;
import types;
import vulkan_renderer;
import camera;
import draw3d;
import mesh;
import simulation;


export class Application {
    public:
        Application() : window(800, 600, "Game"), 
                        event_manager(&window),
                        renderer(window), draw(renderer),
                        simulationThread([this](std::stop_token stop){
                            simulation.runSimulation(stop);
                        })
            {
           camera_controller.registerCamera(
            Camera(glm::vec3(2.0f, 2.0f, 2.0f), 
            glm::vec3(0.0f, 0.0f, 0.0f), 
            glm::vec2(800.0, 600.0), 
            "OrbitCamera"))
            .setActiveCamera("OrbitCamera");

            imguiSetup(window.getSDLWindow());

            }
  

        void loop() {
            const u32 SAMPLES = 500;
            auto start = std::chrono::high_resolution_clock::now();
            auto end = start;

            u32 sampleCount = 0;
            f64 accum = 0.0;
            f64 fps = 0.0;
            float dt = 0.0;
            
           while (window.isWindowOpen())
            {
               
                start = std::chrono::high_resolution_clock::now(); 
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    window.setWindowOpen(false);
                } 


                selectObject();
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();
                ImGui::Begin("Application Info");
                ImGui::Text("FPS (%u samples): %0.1f", SAMPLES, fps);
                ImGui::End();

                ImGui::Begin("Simulation Info");
                ImGui::Text("Simulation Speed: %0.1lf hz", simulation.getSimulationHz());
                ImGui::End();

                ImGui::Render();
                
                draw.startDraw();
                for (const auto object : simulation.getObjectPositions())
                {
                    draw.drawSphere(object, 1.0);
                }
                // Draws the grid
                draw.drawQuad(glm::vec3(0.0, 0.0, 0.0), glm::vec3(50.0f));
                draw.endDraw(*camera_controller.getActiveCamera());
                i32 w, h;
                SDL_GetWindowSize(window.getSDLWindow(), &w, &h);
                camera_controller.getActiveCamera()->update(glm::vec2(w, h));
                camera_controller.updateActiveCamera(event_manager);
               
                
                end = std::chrono::high_resolution_clock::now();
                dt = std::chrono::duration<float>(end - start).count();
                accum += dt;
                sampleCount++;
                if (sampleCount >= SAMPLES)
                {
                    fps = 1.0 / (accum / SAMPLES);
                    sampleCount = 0;
                    accum = 0.0;
                }  
                
            }

            renderer.waitIdle();
            
        }

        ~Application() {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }
      
    private:
       auto imguiSetup(SDL_Window* window) -> void {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            ImGuiStyle &style = ImGui::GetStyle();
            float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
            style.ScaleAllSizes(main_scale);
            style.FontScaleDpi = main_scale;

            ImGui_ImplSDL3_InitForVulkan(window);
            VulkanInfo infoStruct = renderer.getRendererInfo();
            ImGui_ImplVulkan_InitInfo initInfo = {};
            initInfo.ApiVersion = VK_API_VERSION_1_4;
            initInfo.Instance = *infoStruct.g_instance;
            initInfo.PhysicalDevice = *infoStruct.g_physicalDevice;
            initInfo.Device = *infoStruct.g_device;
            initInfo.Queue = *infoStruct.g_queue;
            initInfo.QueueFamily = infoStruct.g_queueFamily;
            initInfo.DescriptorPool = *infoStruct.g_descriptorPool;
            initInfo.MinImageCount = infoStruct.g_minImageCount;
            initInfo.ImageCount = infoStruct.g_ImageCount;

            initInfo.UseDynamicRendering = true;
            initInfo.PipelineInfoMain.RenderPass = nullptr;
            initInfo.PipelineInfoMain.Subpass = 0;
            initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_8_BIT;


            initInfo.CheckVkResultFn = [](VkResult result) {
            if (result != VK_SUCCESS) {
                std::println("ImGui Vulkan error: {}", static_cast<int>(result));
            }
            };

            VkFormat colorFormat = static_cast<VkFormat>(renderer.getSwapSurfaceFormat().format);
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount =1 ;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;   //vk::Format::eD32Sfloat;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;

            ImGui_ImplVulkan_Init(&initInfo);
        }

        auto selectObject() -> void {
            auto mouse = event_manager.getMouseState(); 
            glm::vec3 nearPoint = glm::unProject(glm::vec3(mouse.mouse_x, mouse.mouse_y, 0.0),
            camera_controller.getActiveCamera()->getViewMatrix(),
            camera_controller.getActiveCamera()->getProjMatrix(), 
            glm::vec4(0.0, 0.0, window.getFramebufferSize().first, window.getFramebufferSize().second));



            glm::vec3 farPoint = glm::unProject(glm::vec3(mouse.mouse_x, mouse.mouse_y, 1.0),
            camera_controller.getActiveCamera()->getViewMatrix(),
            camera_controller.getActiveCamera()->getProjMatrix(), 
            glm::vec4(0.0, 0.0, window.getFramebufferSize().first, window.getFramebufferSize().second));

            glm::vec3 raydir = glm::normalize(farPoint - nearPoint);
            const glm::vec3 testpos = glm::vec3(0.0);

            auto const distance_from_sphere = nearPoint - testpos;
            const float discrim = 
            pow(glm::dot(raydir, distance_from_sphere), 2.0) -
            (pow(glm::length(distance_from_sphere), 2.0) - 1.0);

     
            std::println("{}",discrim);
            
            if (discrim >= 0)
            {
                std::println("HIGHLIGHT");
            } 

        }

    
        Window window;
        EventManager event_manager;
        VulkanRenderer renderer;
        GravitySimulation simulation;
        draw::Draw3D draw;
        CameraController camera_controller;
        std::jthread simulationThread;
        
};
