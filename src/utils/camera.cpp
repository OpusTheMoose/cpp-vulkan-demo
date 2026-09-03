module;
#include <cassert>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <print>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <iostream>
#include <string>
#include <utility>

export module camera;
import logger;
import event;

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
        Camera(glm::vec3 pos, glm::vec3 center, glm::vec2 screen_size, std::string label){
            this->pos = pos;
            this->center = center;
            this->label = label;
            view = glm::lookAt(pos, center, glm::vec3(0.0, 0.0 , 1.0));
            proj = glm::perspective(glm::radians(45.0f), screen_size.x / screen_size.y, 0.1f, 100.0f);
            proj[1][1] *= -1; // Flip Y for vulkan

            
    }
    auto getViewMatrix() -> glm::mat4  {
        return view; 
    }
    auto getProjMatrix() ->  glm::mat4 {
        return proj;
    }
    auto zoom(const float zoomAmount) -> void {
     
       radius -= zoomAmount;
    }

     auto getEye() const {
     
        const float x = center.x + radius * cos(polarAngle) * cos(azimuthAngle);
        const float y = center.y + radius * cos(polarAngle) * sin(azimuthAngle);
        const float z = center.z + radius * sin(polarAngle);

        return glm::vec3(x, y, z);
    }

    auto update(const glm::vec2 screen_size)-> void
    {
        // view = glm::lookAt(pos, lookat, glm::vec3(0.0, 0.0 , 1.0));
        view = glm::lookAt(getEye(), center, glm::vec3(0.0, 0.0, 1.0));
        proj = glm::perspective(glm::radians(45.0f), static_cast<float>(screen_size.x) / static_cast<float>(screen_size.y), 0.1f, 100.0f);
        proj[1][1] *= -1; // Flip Y for vulkan
    }

    auto getLabel() -> std::string_view
    {
        return label;
    }
    
    auto rotateAzimuth(const float radians)
    {
        azimuthAngle += radians;

        constexpr float fullCircle = 2.0*glm::pi<float>();
        azimuthAngle = fmodf(azimuthAngle, fullCircle);
        if (azimuthAngle < 0.0) azimuthAngle += fullCircle;
    }

    auto rotatePolar(const float radians) -> void {
        polarAngle += radians;
        constexpr float polarCap = glm::pi<float>() / 2.0f - 0.001f;
        if (polarAngle > polarCap) polarAngle = polarCap;
        if (polarAngle < -polarCap) polarAngle = -polarCap;


    }

   
    // auto processMouseInput(float mouseX, float mouseY) -> void {

    // }

    private:
        std::string label;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 pos;
        glm::vec3 center;

        float azimuthAngle = 0.0f;
        float polarAngle = 45.0f;
        float radius = 2.0f;
};

export class CameraController {
    public:
        CameraController() = default;
        auto& registerCamera(Camera camera)
        {
          std::pair return_val = registeredCameras.emplace(camera.getLabel(), std::move(camera));
          if (return_val.second == false)
          {
            logger.log(LogLevel::INFO, "Failed to register camera in CameraController");
            return *this;
          }
          return *this;

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
        auto updateActiveCamera(EventManager &events)
        {
            Camera* cam = getCamera(active_camera);
            cam->zoom(events.getMouseState().scroll_amount * 0.1);
            if (events.isKeyPressed("left"))  cam->rotateAzimuth(glm::radians(0.1f));
            if (events.isKeyPressed("right")) cam->rotateAzimuth(glm::radians(-0.1f));
            if (events.isKeyPressed("up"))    cam->rotatePolar(glm::radians(0.1f));
            if (events.isKeyPressed("down"))  cam->rotatePolar(glm::radians(-0.1f));

            
        }
   

    private:
        std::string active_camera;
        std::unordered_map<std::string, Camera> registeredCameras;
        Logger logger;
};