module;
#include <stdexcept>
#include <cassert>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_scancode.h"
#include <iostream>
export module event;
import window;
import logger;

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
            while (SDL_PollEvent(&e))
            {
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

    private:
        std::array<bool, SDL_SCANCODE_COUNT> keys = {false};
        Window* window;
        Logger logger;
        




};