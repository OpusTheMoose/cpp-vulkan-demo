module;
#include <print>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
export module app;
import window;
import event;
import types;
import vulkan_renderer;
import camera;
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

           
            meshes.emplace_back();

            for (auto& mesh : meshes) renderer.upload(mesh);
            


        }
        void loop() {
           while (window.isWindowOpen())
            {
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    window.setWindowOpen(false);
                } 

                i32 w, h;
                SDL_GetWindowSize(window.getSDLWindow(), &w, &h);
                camera_controller.getActiveCamera()->update(glm::vec2(w, h));
                camera_controller.updateActiveCamera(event_manager);
                renderer.drawFrame(*camera_controller.getActiveCamera(), meshes);
                
            }
            renderer.waitIdle();
            
        }

        ~Application() = default;
      
    private:
        Window window;
        EventManager event_manager;
        VulkanRenderer renderer;
         
        CameraController camera_controller;
        std::vector<Mesh> meshes;
};
