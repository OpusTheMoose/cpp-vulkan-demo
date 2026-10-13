module;

#include <cassert>
#include <cmath>
#include <glm/ext/vector_float3_precision.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <print>
#include <string_view>
#include <unordered_map>
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
        Camera(glm::dvec3 pos, glm::dvec3 center, glm::vec2 screen_size, double zoom, std::string label){
            this->pos = pos;
            this->center = center;
            this->label = label;
            this->radius = zoom;
            view = glm::lookAt(pos, center, glm::dvec3(0.0, 1.0 , 0.0));
            proj = glm::perspectiveRH_ZO(glm::radians(45.0f), 
                                        screen_size.x / screen_size.y, 
                                        static_cast<float>(nearPlane), 
                                        static_cast<float>(farPlane));
            proj[1][1] *= -1; // Flip Y for vulkan

            
    }
    auto getViewMatrix() -> glm::mat4  {
        return view; 
    }
    auto getProjMatrix() ->  glm::mat4 {
        return proj;
    }
    auto getCenter() -> const glm::dvec3 {
        return center;
    }
  
    auto zoom(const float zoomAmount) -> void {
     
        constexpr double minRadius = 0.1;
        constexpr double maxRadius = 1e9;
       //Flip so scroll up is zoom in
       double factor = std::exp(zoomAmount * -1);
       radius *= factor;
       radius = glm::clamp(radius, minRadius, maxRadius);
    }
    auto getZoom() const -> double {
        return radius;
    }

     auto getEye() const {
     
        const double x = center.x + radius * cos(polarAngle) * cos(azimuthAngle);
        const double z = center.z + radius * cos(polarAngle) * sin(azimuthAngle);
        const double y = center.y + radius * sin(polarAngle);

        return glm::dvec3(x, y, z);
    }
      auto getPosition() const -> glm::dvec3 {
        return getEye();
    }
    auto update(const glm::vec2 screen_size)-> void
    {
     
        view = glm::lookAt(getEye(), static_cast<glm::dvec3>(center), glm::dvec3(0.0, 1.0, 0.0));
        proj = glm::perspectiveRH_ZO(glm::radians(45.0f), 
        static_cast<float>(screen_size.x) / static_cast<float>(screen_size.y), 
        static_cast<float>(nearPlane), 
        static_cast<float>(farPlane));
        proj[1][1] *= -1; // Flip Y for vulkan

    }

    auto getLabel() -> std::string_view
    {
        return label;
    }
    
    auto rotateAzimuth(const double radians)
    {
        azimuthAngle += radians;

        constexpr double fullCircle = 2.0*glm::pi<double>();
        azimuthAngle = fmod(azimuthAngle, fullCircle);
        if (azimuthAngle < 0.0) azimuthAngle += fullCircle;
    }

    auto rotatePolar(const double radians) -> void {
        polarAngle += radians;
        constexpr double polarCap = glm::pi<double>() / 2.0 - 0.001;
        if (polarAngle > polarCap) polarAngle = polarCap;
        if (polarAngle < -polarCap) polarAngle = -polarCap;


    }

    auto translatePosition(glm::vec3 translation)
    {
        center += translation;
    }
    auto getPolarAngle() {return polarAngle; }
    auto getAzimuthAngle() {return azimuthAngle; }


    private:
        std::string label;
        glm::mat4 view;
        glm::mat4 proj;
        glm::dvec3 pos;
        glm::dvec3 center;

        double nearPlane = 1.0e12f;
        double farPlane = 0.1f;

        double azimuthAngle = 0.0;
        double polarAngle = glm::radians(45.0);
        double radius = 20.0;
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
   
            constexpr double sensitivity = 0.005f; // radians per pixel

            const auto& mouse = events.getMouseState();
            if (mouse.isMiddleButtonPressed() && !events.isKeyPressed("left shift")) {
                cam->rotateAzimuth((mouse.mouse_x - mouse.prev_mouse_x) * sensitivity);
                cam->rotatePolar((mouse.mouse_y - mouse.prev_mouse_y) * sensitivity);
                
               
            }
            if (events.isKeyPressed("left shift") && mouse.isMiddleButtonPressed())
            {
                // Tie how far left/right we move to our zoom level
                double move_sens = cam->getZoom() * 0.0005;
                auto const delta_x = mouse.mouse_x - mouse.prev_mouse_x;
                auto const delta_y = mouse.mouse_y - mouse.prev_mouse_y;

                // Derivative of the azimuth angle determines how much left-right we move
                auto phi_hat = glm::vec3(-sin(cam->getAzimuthAngle()), 0.0, cos(cam->getAzimuthAngle()));

                cam->translatePosition(glm::vec3(
                    phi_hat.x * delta_x * move_sens, 
                    delta_y *  move_sens, 
                    phi_hat.z * delta_x * move_sens));
            }
         
        }
   

    private:
        std::string active_camera;
        std::unordered_map<std::string, Camera> registeredCameras;
        Logger logger;
};