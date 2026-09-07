module;
#include <glm/fwd.hpp>
#include <vector>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/matrix.hpp>

export module draw3d;
import vertex;
import types;
import mesh;
import vulkan_renderer;
import camera;

namespace draw{

export enum PrimitiveType {
        LINE3D = 0,
        SPHERE = 1,
        CUBE = 2,
        CYLINDER = 3

};

export class Draw3D {
    public:
        Draw3D() = default;
        Draw3D(VulkanRenderer &renderer) {
            this->renderer = &renderer;
            for (auto &e : instanceBuffers)
            {
               e = renderer.createInstanceBuffer(sizeof(glm::mat4) * 1000);
            }
            generateSphereData();

        }

        auto startDraw() {
            sphereInstanceData.clear();

        }
        // Submit for rendering
       auto endDraw(Camera camera) {
        renderer->drawFrame(camera, meshes[PrimitiveType::SPHERE], instanceBuffers[PrimitiveType::SPHERE], sphereInstanceData);
        
       }

       auto drawSphere(const glm::vec3 pos, const f32 radius) -> void {
            sphereInstanceData.emplace_back(createModelMatrix(pos, glm::vec3{radius, radius, radius}, glm::quat(0.0f, 0.0f, 0.0f, 1.0)));
            return;
       }

              

    Mesh& getMesh(PrimitiveType type)
    {
        return meshes[type];
    }
    private:

     auto generateSphereData() -> void
       {

        const i32 subdivisions = 20;
        const i32 stacks = std::max(3, subdivisions); 
        const i32 slices = std::max(3, subdivisions * 2); 

        std::vector<Vertex> sphereVertices;

        for (i32 i = 0; i <= stacks; ++i)
        {
            // phi: 0 at north pole, PI at south pole
            const float phi = static_cast<float>(i) / static_cast<float>(stacks) * static_cast<float>(M_PI);
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);

            for (i32 j = 0; j <= slices; ++j)
            {
            // theta: 0 to 2PI around the sphere
            const float theta = static_cast<float>(j) / static_cast<float>(slices) * 2.0f * static_cast<float>(M_PI);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            // Position on unit sphere, scaled by radius
            const float x = sinPhi * cosTheta;
            const float y = cosPhi;
            const float z = sinPhi * sinTheta;

            Vertex vert{};
            vert.pos = {x, y, z};
            vert.normal = {x, y, z};
            vert.color = {1.0, 0.0, 0.0};

            // UV: u wraps around, v goes from top (0) to bottom (1)
            vert.uv.x = static_cast<float>(j) / static_cast<float>(slices);
            vert.uv.y = static_cast<float>(i) / static_cast<float>(stacks);

            sphereVertices.emplace_back(vert);
        }
    }

    // --- Generate indices ---
    // Each row has (slices + 1) vertices due to the seam duplication for UVs
    const int32_t vertsPerRow = slices + 1;
    std::vector<u16> sphereIndices;

    for (int32_t i = 0; i < stacks; ++i)
    {
        for (int32_t j = 0; j < slices; ++j)
        {
            const uint32_t topLeft     = i * vertsPerRow + j;
            const uint32_t topRight    = topLeft + 1;
            const uint32_t bottomLeft  = (i + 1) * vertsPerRow + j;
            const uint32_t bottomRight = bottomLeft + 1;

            // Skip degenerate triangles at poles (optional optimization)
            if (i != 0)
            {
                sphereIndices.push_back(topLeft);
                sphereIndices.push_back(bottomLeft);
                sphereIndices.push_back(topRight);
            }

            if (i != stacks - 1)
            {
                sphereIndices.push_back(topRight);
                sphereIndices.push_back(bottomLeft);
                sphereIndices.push_back(bottomRight);
            }
        }
    }
        
        meshes[PrimitiveType::SPHERE] = Mesh(sphereVertices, sphereIndices);

        // Creates VBO and IBO stuff for vulkan
        renderer->upload(meshes[PrimitiveType::SPHERE], instanceBuffers[PrimitiveType::SPHERE]);
    }

    inline auto createModelMatrix(const glm::vec3 pos, const glm::vec3 scale, const glm::quat rot) -> glm::mat4 {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model *= glm::mat4_cast(rot);             // apply quaternion rotation
            model = glm::scale(model, scale);
            return model;
        }



    VulkanRenderer *renderer;
    std::array<Mesh, static_cast<int>(PrimitiveType::CYLINDER) + 1> meshes;
    std::array<InstanceBuffer, static_cast<int>(PrimitiveType::CYLINDER)+1> instanceBuffers; 

    std::vector<glm::mat4> sphereInstanceData;
  
};

}