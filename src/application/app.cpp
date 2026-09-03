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


export class Application {
    public:
        Application() {
            this->window = new Window(800, 600, "Game");
           event_manager = EventManager(this->window);
           renderer = VulkanRenderer(*window);
           camera_controller.registerCamera(
            Camera(glm::vec3(2.0f, 2.0f, 2.0f), 
            glm::vec3(0.0f, 0.0f, 0.0f), 
            glm::vec2(800.0, 600.0), 
            "OrbitCamera"))
            .setActiveCamera("OrbitCamera");


        }
        void loop() {
           while (window->isWindowOpen())
            {
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    window->setWindowOpen(false);
                } 

                i32 w, h;
                SDL_GetWindowSize(window->getSDLWindow(), &w, &h);
                camera_controller.getActiveCamera()->update(glm::vec2(w, h));
                camera_controller.updateActiveCamera(event_manager);
                renderer.drawFrame(*camera_controller.getActiveCamera());
                
            }
            renderer.waitIdle();
            
        }
        ~Application() {
            delete this->window;

        }
    private:
        VulkanRenderer renderer;
        Window* window;
        EventManager event_manager; 
        CameraController camera_controller;
};
