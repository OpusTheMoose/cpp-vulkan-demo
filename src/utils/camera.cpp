module;
#include <cassert>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <string>
#include <utility>

export module camera;
import logger;

export enum class CameraMovement
{
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down
};



export class Camera {
    public:
        Camera() = default;
        Camera(glm::vec3 pos, glm::vec3 lookat, glm::vec2 screen_size, std::string label){
            this->pos = pos;
            this->lookat = lookat;
            this->label = label;
            view = glm::lookAt(pos, lookat, glm::vec3(0.0, 0.0 , 1.0));
            proj = glm::perspective(glm::radians(45.0f), screen_size.x / screen_size.y, 0.1f, 100.0f);
            proj[1][1] *= -1; // Flip Y for vulkan

            
    }
    auto getViewMatrix() -> glm::mat4  {
        return view; 
    }
    auto getProjMatrix() ->  glm::mat4 {
        return proj;
    }
    auto zoom(float zoomFactor) -> void {
        glm::vec3 dir = glm::normalize(lookat - pos);
        pos += dir * zoomFactor;


    }
    auto update(glm::vec2 screen_size)-> void
    {
        proj = glm::perspective(glm::radians(45.0f), static_cast<float>(screen_size.x) / static_cast<float>(screen_size.y), 0.1f, 100.0f);
        proj[1][1] *= -1; // Flip Y for vulkan
    }

    auto getLabel() -> std::string_view
    {
        return label;
    }
    
    // auto processMouseInput(float mouseX, float mouseY) -> void {

    // }

    private:
        std::string label;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 pos;
        glm::vec3 lookat;
};

export class CameraController {
    public:
        CameraController() = default;
        auto registerCamera(Camera camera)
        {
          std::pair return_val = registeredCameras.emplace(camera.getLabel(), std::move(camera));
          if (return_val.second == false)
          {
            logger.log(LogLevel::INFO, "Failed to register camera in CameraController");
            return;
          }

        }
        auto setActiveCamera(const std::string &camera_label) -> void {
            this->active_camera = camera_label;
        }
        auto getActiveCamera() -> Camera*
        {
            return getCamera(active_camera);
        }
        auto getCamera(const std::string &label) -> Camera* {
            auto it = registeredCameras.find(label);
            if (it == registeredCameras.end()) {
                logger.log(LogLevel::ERROR, "Camera not found!");
                return nullptr;
            }
            return &it->second;

        }
   

    private:
        std::string active_camera;
        std::unordered_map<std::string, Camera> registeredCameras;
        Logger logger;
};