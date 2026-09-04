module;
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <SDL3/SDL.h>
#include <iostream>
export module app;
import window;
import event;
import types;
import vulkan_renderer;
import camera;
import mesh;
import sphere;


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

            Sphere new_sphere(1.0, 20, glm::vec3(1.0, 0.0, 0.0), glm::vec3(1.0f, 0.0f, 0.0f));
            Sphere new_sphere_2(1.0, 20, glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 2.0));

           
            meshes.emplace_back(std::move(new_sphere.getMesh()));
            meshes.emplace_back(std::move(new_sphere_2.getMesh()));

             for (auto& mesh : meshes) 
             {
                renderer.upload(mesh);
               
             }
            


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
