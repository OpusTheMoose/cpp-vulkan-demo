module;
#include <print>
export module app;
import window;
import event;
import vulkan_renderer;


export class Application {
    public:
        Application() {
            this->window = new Window(800, 600, "Game");
           event_manager = EventManager(this->window);
           renderer = VulkanRenderer(*window);
           

          
        }
        void loop() {
           while (window->isWindowOpen())
            {
                event_manager.pollEvents();
                
                if (event_manager.isKeyPressed("escape"))
                {
                    std::println("Pressed!");
                    window->setWindowOpen(false);
                } 

                renderer.drawFrame();
            }
            renderer.waitIdle();
            
        }
        ~Application() {
            delete this->window;

        }
    private:
        Window *window;
        EventManager event_manager; 
        VulkanRenderer renderer;
};
