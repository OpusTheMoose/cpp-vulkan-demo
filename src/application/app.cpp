module;
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <SDL3/SDL.h>
#include <print>
export module app;
import window;
import event;
import types;
import vulkan_renderer;
import camera;
import draw3d;
import mesh;


export class Application {
    public:
        Application() : window(800, 600, "Game"), 
                        event_manager(&window),
                        renderer(window)
            {
           camera_controller.registerCamera(
            Camera(glm::vec3(2.0f, 2.0f, 2.0f), 
            glm::vec3(0.0f, 0.0f, 0.0f), 
            glm::vec2(800.0, 600.0), 
            "OrbitCamera"))
            .setActiveCamera("OrbitCamera");

            draw = draw::Draw3D(renderer);

       


            }
  

        void loop() {
            // std::println("Vert color: {} {} {}", meshes.at(0)->getVertices().at(0));
           while (window.isWindowOpen())
            {
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    window.setWindowOpen(false);
                } 

                draw.startDraw();
                draw.drawSphere(glm::vec3{0.0, 0.0, 0.0}, 1.0);
                draw.drawSphere(glm::vec3{2.0, 0.0, 0.0}, 2.0);
                draw.endDraw(*camera_controller.getActiveCamera());
                
                
                i32 w, h;
                SDL_GetWindowSize(window.getSDLWindow(), &w, &h);
                camera_controller.getActiveCamera()->update(glm::vec2(w, h));
                camera_controller.updateActiveCamera(event_manager);
                
                
                
            }
            renderer.waitIdle();
            
        }

        ~Application() = default;
      
    private:
        Window window;
        EventManager event_manager;
        VulkanRenderer renderer;
        draw::Draw3D draw;
         
        CameraController camera_controller;
        
};
