module;
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
export module window;
import types;
import logger;

export class Window {
    public: 
        Window() {};
        Window (const Window& ) = delete;
        Window& operator= (const Window&) = delete;
        Window(u32 width, u32 height, const std::string &title )
        {
            this->width = width;
            this->height = height;

            try {
                if (!SDL_Init(SDL_INIT_VIDEO)) {
                    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
                }

               this->sdl_window = std::unique_ptr<SDL_Window, SDL_WindowDeleter>(
                SDL_CreateWindow(title.c_str(), this->width, this->height, 
                SDL_WINDOW_ALWAYS_ON_TOP | 
                SDL_WINDOW_RESIZABLE | 
                SDL_WINDOW_VULKAN));

                if (!this->sdl_window.get()) throw std::runtime_error(std::string("Failed to create window") + SDL_GetError());
            }

            catch(const std::exception& e)
            {
                logger.log(LogLevel::CRITICAL, e.what());
                return;
            }

            logger.log(LogLevel::INFO, "Window created successfully");

        }
        auto isFramebufferResized() -> bool {
            return resized;
        }
        auto setFramebufferResized(bool resized) -> void {
            this->resized = resized;
        }
        auto setWindowOpen(bool isOpen) -> void { this->isOpen = isOpen; }
        auto isWindowOpen() -> bool {return this->isOpen; }
        auto getSDLWindow() -> SDL_Window* {
            return sdl_window.get();
        }
        auto getFramebufferSize()
        {
            i32 w, h;
            SDL_GetWindowSizeInPixels(sdl_window.get(), &w, &h );
            return std::tuple<int, int>(w, h);
        }
        ~Window() { 
            SDL_Quit();
        }
    private:
        struct SDL_WindowDeleter {
            void operator()(SDL_Window* window) const { 
                SDL_DestroyWindow(window);
            }
        };
    

        std::unique_ptr<SDL_Window, SDL_WindowDeleter> sdl_window;
        
        u32 width, height;
        bool isOpen = true;
        bool resized = false;

        Logger logger;
};
