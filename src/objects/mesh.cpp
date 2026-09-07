module;
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan_raii.hpp>
export module mesh;
import vertex;
import types;

export class Mesh {
    public:
        Mesh() = default;

         Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) noexcept = default;
        Mesh& operator=(Mesh&&) noexcept = default;

        /*
            VERTICES AND INDICES ARE MOVED WHEN USING THE PARAMERTIZED CONSTRUCTOR
            THIS IS NOT A GOOD DESIGN SO BE CAREFUL
        */
        Mesh(std::vector<Vertex> &vertices, std::vector<u16> &indices)
        {
            this->vertices = std::move(vertices);
            this->indices = std::move(indices);
            
        }
       
        auto upload(std::vector<Vertex> &vertices_, std::vector<u16> &indices_){
            this->vertices = vertices_;
            this->indices = indices_;
        }

        auto setScale(glm::vec3 scale) {
            this->scale = scale;
        }
        auto setPosition(glm::vec3 pos) {
            this->pos = pos;
        }

        std::vector<Vertex> getVertices() {
            return vertices;
        }

        std::vector<u16> getIndices() {
            return indices;
        }

        auto getModelMatrix() -> glm::mat4 {
             glm::mat4 model = glm::translate(glm::mat4(1.0f), this->pos);
            model *= glm::mat4_cast(this->rot);             // apply quaternion rotation
            model = glm::scale(model, this->scale);
            return model;
        }
     
                
       vk::raii::Buffer vertexBuffer = nullptr;
       vk::raii::DeviceMemory vertexBufferMemory = nullptr; 
       vk::raii::Buffer indexBuffer = nullptr;
       vk::raii::DeviceMemory indexBufferMemory = nullptr;
       std::vector<vk::raii::DescriptorSet> descriptorSets;
    private:
        glm::vec3 pos = {0.0, 0.0, 0.0};
        glm::vec3 scale = {1.0, 1.0, 1.0};
        glm::quat rot = {0.0f, 0.0f, 0.0f, 1.0f};
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
};

export struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};
