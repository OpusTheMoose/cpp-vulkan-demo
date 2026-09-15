module;
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <print>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <SDL3/SDL.h>
#include <chrono>
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
            const u32 SAMPLES = 500;
            auto start = std::chrono::high_resolution_clock::now();
            auto end = start;

            u32 sampleCount = 0;
            f64 accum = 0.0;
           while (window.isWindowOpen())
            {
               
                start = std::chrono::high_resolution_clock::now(); 
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    window.setWindowOpen(false);
                } 

                draw.startDraw();
                for (u32 i = 0; i < 30; i++)
                {
                    for (u32 j = 0; j < 30; j++)
                    {
                        draw.drawSphere(glm::vec3{static_cast<float>(2.0 * i) - 10.0, 0.0, static_cast<float>(2.0 * j) - 10.0}, 1.0);
                    }
                }
                draw.drawQuad(glm::vec3(0.0, 0.0, 0.0), glm::vec3(50.0f));
                // sphere with light
                draw.drawSphere(glm::vec3(0.0, -5.0, 0.0), 0.5);
                
                draw.endDraw(*camera_controller.getActiveCamera());
                
                
                i32 w, h;
                SDL_GetWindowSize(window.getSDLWindow(), &w, &h);
                camera_controller.getActiveCamera()->update(glm::vec2(w, h));
                camera_controller.updateActiveCamera(event_manager);
               
                
                end = std::chrono::high_resolution_clock::now();
                auto delta = std::chrono::duration<double>(end - start);
                accum += delta.count();
                sampleCount++;
                if (sampleCount >= SAMPLES)
                {
                    std::println("{} FPS over {} samples", 1.0 / (accum / SAMPLES) , SAMPLES);
                    sampleCount = 0;
                    accum = 0.0;
                }  
                
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
