module;
#include "SDL3/SDL_video.h"
#include <array>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <print>
#include <stop_token>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <print>
#include <thread>
#include <tracy/Tracy.hpp>

#include <imgui.h>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

export module app;
import vertex;
import window;
import event;
import types;
import vulkan_renderer;
import camera;
import draw3d;
import mesh;
import simulation;
import raycast;

constexpr u32 MAX_OBJECT_COUNT = 128;


class Grid {
    public:
        Grid(VulkanRenderer& renderer)
        {
            createMeshVertices(renderer);
            createGridPipeline(renderer);
            quad.setPosition(glm::vec3(0.0));
            quad.setScale(glm::vec3(1e9));
        }
        auto& getMesh() {return quad; }
        auto& getPipeline() {return gridPipeline; }
        const auto& getBuffer() {return UBO; }

    private:
        auto createMeshVertices(VulkanRenderer &renderer) ->void {
        std::vector<Vertex> quadVertices;
        std::vector<u16> indices;
        // top left
        quadVertices.emplace_back(glm::vec3{-1.0, 0.0, 1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{0.0, 0.0}, glm::vec3{0.0, 1.0, 0.0} );
        // bottom left
        quadVertices.emplace_back(glm::vec3{-1.0, 0.0, -1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{0.0, 1.0}, glm::vec3{0.0, 1.0, 0.0} );
        // bottom right
        quadVertices.emplace_back(glm::vec3{1.0, 0.0, -1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 1.0}, glm::vec3{0.0, 1.0, 0.0} );
        // top right
        quadVertices.emplace_back(glm::vec3{1.0, 0.0, 1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 0.0}, glm::vec3{0.0, 1.0, 0.0} );

        indices = {
            0, 2, 1, 0, 3, 2
        };
      
        quad.upload(quadVertices, indices);
        quad.vertexBuffer = renderer.createVertexBuffer(quadVertices.data(), quadVertices.size() * sizeof(Vertex));
        quad.indexBuffer = renderer.createIndexBuffer(indices.data(), indices.size() * sizeof(u16));

        }
        auto createGridPipeline(VulkanRenderer& renderer) -> void {
            PipelineBuilder pipelineBuilder(renderer);
            gridPipeline = pipelineBuilder
            .setColorAttachmentFormats(std::array{renderer.getSwapSurfaceFormat().format})
            .setRenderingColorAttachmentCount(1)
            .setColorBlendAttachment(false, vk::ColorComponentFlagBits::eR | 
                                           vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB |
                                           vk::ColorComponentFlagBits::eA
                                        
                                        )
            .setColorBlendState()
            //Depth
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
            .setDepthClampEnable(vk::False)
            .setDepthTest(vk::True)
            .setDepthWrite(true)
            .setStencilTest(vk::False)
            // Reasterizer
            .setDepthBiasEnable(vk::False)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setLineWidth(1.0f)
            .setDiscardEnable(vk::False)
            .setMultisampleSamples(vk::SampleCountFlagBits::e8)
            // Input Assembly
            .setInputAssembly(vk::PrimitiveTopology::eTriangleList)
            .setShader("src/shaders/grid_floor.spv", vk::ShaderStageFlagBits::eVertex, "vertMain")
            .setShader("src/shaders/grid_floor.spv", vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setVertexInputInfo(Vertex::getBindingDescription(),Vertex::getAttributeDescriptions())
            .setViewportState(1, 1)
            .setMultisampleShading(false)
            .build();

            UBO = renderer.createUniformBuffers(sizeof(glm::mat4) * 2);

            for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; frame++)
            {
                renderer.uploadBuffer(gridPipeline, 
                    UBO, 
                    vk::DescriptorType::eUniformBuffer, 
                    0, 
                    0, 
                    frame);
            }


        }

   
        Buffer UBO;
        Mesh quad;
        GraphicsPipeline gridPipeline;

};

constexpr std::string doubleToScientific(double value) {
   return std::format("{:.0e} ", value);
}


export class Application {
    public:
        Application() : window(800, 600, "Game"), 
                        event_manager(&window),
                        renderer(window), draw(renderer),
                        simulationThread([this](std::stop_token stop){
                            simulation.runSimulation(stop);
                        }),
                        grid(renderer)
            {

            glm::dvec3 startCameraCenter = glm::dvec3(0.0, 0.0, 0.0);
            glm::dvec3 startCameraPos = glm::dvec3(0.0, 0.0, 0.0);
            double zoom = 1.0f;

            const u32 simulationObjectCount = simulation.getSimulationObjectCount();

            if (simulationObjectCount > 0)
            {
                auto object = simulation.getObjectFromId(0);
                startCameraCenter = object.position;
                startCameraPos = glm::dvec3(0.0); // doesnt actually matter, gets recaclulated 
                zoom = object.radius * 10;

                selectedObject = simulation.getObjectFromId(0);
                focusedObject = selectedObject;

                
            }
            for (u32 i = 0; i < simulationObjectCount; i++)
            {
                positionHistory[i].push_back(simulation.getObjectFromId(i).position);
            }
           camera_controller.registerCamera(
            Camera(startCameraPos, 
            startCameraCenter, 
            glm::vec2(800.0, 600.0), 
            zoom,
            "OrbitCamera"))
            .setActiveCamera("OrbitCamera");

            imguiSetup(window.getSDLWindow());

            draw.uploadTexture("textures/2k_mercury.jpg");
            draw.uploadTexture("textures/2k_earth.jpg");



            }

            
  

        void loop() {
            const u32 SAMPLES = 500;
            auto start = std::chrono::high_resolution_clock::now();
            auto end = start;

            u32 sampleCount = 0;
            f64 accum = 0.0;
            
            f64 fps = 0.0;
            float dt = 0.0;
            int hrs = 0, minutes = 0, seconds = 1;

            bool gridToggleOn = true;
            bool paused = false;

            

           while (window.isWindowOpen())
            {
               
                ZoneScopedN("Application frame");
                start = std::chrono::high_resolution_clock::now();
                { 
                    ZoneScopedN("Poll events");
                    event_manager.pollEvents();
                }
                 i32 w, h;
                SDL_GetWindowSize(window.getSDLWindow(), &w, &h);
                
               
           
                if (event_manager.isKeyPressed("escape"))
                {
                    window.setWindowOpen(false);
                } 
                
                {
                ZoneScopedN("ImGUI Application Info")
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();
                ImGui::Begin("Application Info");
                ImGui::Text("FPS (%u samples): %0.1f", SAMPLES, fps);
                auto zoomString = doubleToScientific(camera_controller.getActiveCamera()->getZoom());
                ImGui::Text("Zoom: %s meters", zoomString.c_str());
                ImGui::Checkbox("Toggle Grid", &gridToggleOn);
                ImGui::End();

                 
                ImGui::Begin("Simulation Info");
                ImGui::Text("Simulation Frequnecy: %0.1lf hz", simulation.getSimulationHz());
                ImGui::Text("Simulation Speed: %lf / s", simulation.getSimulationSpeed());

                ImGui::SetNextItemWidth(80);
                ImGui::InputInt("H", &hrs);

                ImGui::SetNextItemWidth(80);
                ImGui::SameLine(120); ImGui::InputInt("M", &minutes);

                ImGui::SetNextItemWidth(80);
                ImGui::SameLine(220); ImGui::InputInt("S", &seconds);

             
                if(ImGui::Button("Pause Simulation")) paused = !paused;
             

                ImGui::End();

                selectObject();

                ImGui::Render();

                simulation.setSimulationSpeed((hrs * 3600.0 + minutes * 60 + seconds) * !paused);
                }

                Camera& activeCamera = *camera_controller.getActiveCamera();
                
                draw.startDraw();
                {
                    ZoneScopedN("Draw submission");
                    for (size_t i = 0; i < simulation.getSimulationObjectCount(); i++)
                    {
                        
                        SimulationObject object = simulation.getObjectFromId(i);
                        
                        draw.drawSphere(object.position, object.radius, object.texID);
                   
             
                    }
                
                    if (gridToggleOn)
                    {
                       draw.drawMesh(grid.getMesh(), grid.getPipeline(), PushConstant(grid.getMesh().getModelMatrix(), vk::ShaderStageFlagBits::eVertex, 0));
                       draw.setUniform(std::array<glm::mat4, 2>{activeCamera.getViewMatrix(), activeCamera.getProjMatrix()}, grid.getBuffer());
                    }

                    traceOrbits(dt);
                   
                 draw.endDraw(activeCamera);
                }
            
                focusedObject = simulation.getObjectFromId(focusedObject.id);
                activeCamera.setCenter(focusedObject.position);
                camera_controller.updateActiveCamera(event_manager, glm::vec2(w, h));

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

                FrameMark;
                
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

        enum SelectState {
            NOT_HIGHLIGHTED,
            HIGHLIGHTED_NOT_SELECTED,
            SELECTED 
        };

        auto selectObject() -> void {

            ZoneScopedN("Select object");
            if(state==SELECTED){

                selectedObject =  simulation.getObjectFromId(selectedObject.id);
                ImGui::Begin("Object");
                ImGui::Text("Position: %lf, %lf, %lf m", selectedObject.position.x, selectedObject.position.y, selectedObject.position.z);
                ImGui::Text("Orbital Velocity : %lf m/s", glm::length(selectedObject.velocity));
                auto s = doubleToScientific(selectedObject.mass);
                ImGui::Text("Mass: %s Kg", s.c_str());
                if(ImGui::Button("Focus"))
                {
                    camera_controller.getActiveCamera()->setCenter(selectedObject.position);
                    focusedObject = selectedObject; 
                    
                }
                ImGui::End();
            }

            auto mouse = event_manager.getMouseState(); 
            auto nearPoint = screenSpaceToWorldCoordinates(glm::vec3(mouse.mouse_x, mouse.mouse_y, 0.0));
            auto farPoint = screenSpaceToWorldCoordinates(glm::vec3(mouse.mouse_x, mouse.mouse_y, 1.0));
            const auto cameraPos = camera_controller.getActiveCamera()->getPosition();
            
            // convert to in-camera coords
            nearPoint -= cameraPos;
            farPoint -= cameraPos;
            

            for (size_t i = 0; i < simulation.getSimulationObjectCount(); i++)
            {
             
                auto const& objectData = simulation.getObjectFromId(i); 

                const bool hit = 
                raycast.testRayOnSphere(
                    Ray(nearPoint, farPoint), 
                    objectData.position - cameraPos, 
                    objectData.radius);
                  
                if (hit)
                {
                    if (mouse.isLeftButtonPressed())
                    {
                        selectedObject = simulation.getObjectFromId(i);
                         state = SELECTED;
                    }
              
                }
                else if (hit == false && (state != SELECTED)) {
                    state=NOT_HIGHLIGHTED;
                }
     
        
                
            }
         

        }

        auto screenSpaceToWorldCoordinates(const glm::vec3 screenCoordinates) -> const glm::vec3 {
           
            
            return glm::unProjectZO(screenCoordinates,
            camera_controller.getActiveCamera()->getViewMatrix(),
            camera_controller.getActiveCamera()->getProjMatrix(), 
            glm::vec4(0.0, 0.0, window.getFramebufferSize().first, window.getFramebufferSize().second));
        }

    

        float orbitTraceTimer = 1.0;
        auto traceOrbits(float dt) -> void {
            ZoneScopedN("Trace orbits");

            auto const simulationObjectCount = simulation.getSimulationObjectCount();

            constexpr size_t MAX_LINE_VERTICES = 8192;
            const size_t MAX_ORBIT_SEGMENTS =
            MAX_LINE_VERTICES / (2 * simulationObjectCount);
            const size_t MAX_ORBIT_POINTS = MAX_ORBIT_SEGMENTS + 1;

            orbitTraceTimer += dt;
  
            for (u32 i = 0; i < simulationObjectCount; i++)
            {
                SimulationObject object = simulation.getObjectFromId(i);
                auto &history = positionHistory[i];
                if (orbitTraceTimer >= 1.0)
                {
                    history.push_back(object.position);
                    if (history.size() > MAX_ORBIT_POINTS)
                    {
                        history.erase(history.begin());
                    }
        
                }

                for (u32 j = 1; j < positionHistory[i].size(); j++)
                {
                    draw.drawLine3D(history[j - 1], history[j], glm::vec3(1.0, 0.0, 0.0));

                }
                if (!history.empty()) positionHistory[i].back() = object.position;
                
            }
            if (orbitTraceTimer >= 1.0) orbitTraceTimer = 0.0;
              
        }
    
        Window window;
        Raycast raycast;
        EventManager event_manager;
        VulkanRenderer renderer;
        GravitySimulation simulation;
        draw::Draw3D draw;
        CameraController camera_controller;
        std::jthread simulationThread;
        GraphicsPipeline gridPipeline;

        SimulationObject selectedObject;
        SimulationObject  focusedObject;
        SelectState state = NOT_HIGHLIGHTED;
        
        Grid grid;

        std::array<std::vector<glm::vec3>, 128> positionHistory;




        
};

