module;
#include <stdexcept>
#include <cassert>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_scancode.h"
#include "imgui_impl_sdl3.h"
#include <iostream>
export module event;
import window;
import logger;
import types;


struct MouseState {
    f32 mouse_x, mouse_y = 0;
    i32 scroll_amount = 0;
};

export class EventManager {
    public:
        EventManager() {};
        EventManager(Window* window) {
            assert(window != nullptr);

            try {
                if (!SDL_Init(SDL_INIT_EVENTS)) {
                    throw std::runtime_error(std::string("SDL_Init Events failed: ") + SDL_GetError());
                }
            }
            catch (const std::exception &e)
            {
                logger.log(LogLevel::CRITICAL, e.what());

                return;
            }
            this->window = window;



  

        }
        // Poll events for windowing, keyboard, mouse etc
        auto pollEvents() {
            SDL_Event e;
            this->mouse.scroll_amount = 0;
            while (SDL_PollEvent(&e))
            {
                ImGui_ImplSDL3_ProcessEvent(&e); 
                switch (e.type)
                {
                    // Key up / down
                    case SDL_EVENT_KEY_DOWN:
                        keys[e.key.scancode] = true;
                        break;
                    case SDL_EVENT_KEY_UP:
                        keys[e.key.scancode] = false;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        window->setWindowOpen(false);
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        window->setFramebufferResized(true);
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        this->mouse.scroll_amount = e.wheel.y;
                        break;
                }
            }
        }
        auto isKeyPressed(const char* key)
        {
            SDL_Scancode key_value = SDL_GetScancodeFromName(key);
            if (key_value == SDL_SCANCODE_UNKNOWN)
            {
                logger.log(LogLevel::WARNING, "Pressed key is invalid.");
                return false;
            }
            
            return keys.at(key_value);
        }

        auto getMouseState() -> MouseState {
            return mouse;
        };


    private:
        std::array<bool, SDL_SCANCODE_COUNT> keys = {false};
        MouseState mouse;
        Window* window;
        Logger logger;
        




};