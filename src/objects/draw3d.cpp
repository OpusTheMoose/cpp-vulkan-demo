module;
#include <array>
#include <glm/fwd.hpp>
#include <vector>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/matrix.hpp>

#include <vulkan/vulkan_raii.hpp>
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
               e = renderer.createStorageBuffer(sizeof(InstanceData) * 1000);
            }

        lightBuffer = renderer.createStorageBuffer(sizeof(Light) * 50);
            // Create uniform buffers BEFORE descriptor sets!!
        uniformBuffer = renderer.createUniformBuffers(sizeof(UniformBufferObject));

        passBuffer = renderer.createStorageBuffer(8); // just pass some bs


        this->descriptorSets = renderer.allocateDescriptorSets();

        for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            renderer.updateDescriptorSets(descriptorSets[i], uniformBuffer, passBuffer, lightBuffer, instanceBuffers[SPHERE], i);
        }
           
        generateSphereData();
    
        }

        auto startDraw() {
            sphereInstanceData.clear();
            lineVertices.clear();
            lineIndices.clear();

        }
        // Submit for rendering
       auto endDraw(Camera camera) {
        Mesh& sphere = meshes[PrimitiveType::SPHERE];
        std::vector<Light> lights;
        lights.emplace_back(glm::vec3(0.0, -5.0, 0.0), glm::vec3(0.8));
       
        DrawSubmission draw = {
            PipelineType::Mesh,
            sphere.vertexBuffer,
            sphere.indexBuffer,
            instanceBuffers[PrimitiveType::SPHERE],
            lightBuffer,
            sphereInstanceData,
            lights,
            descriptorSets,
            static_cast<u32>(sphere.getIndices().size()),
            static_cast<u32>(sphereInstanceData.size())
        };
 
        renderer->drawFrame(camera, uniformBuffer, draw);
        
       }

       auto drawLine3D(const glm::vec3 start, const glm::vec3 end, const glm::vec3 color)
       {
            lineVertices.emplace_back(start, color);
            lineVertices.emplace_back(end, color);
            lineIndices.push_back(lineVertices.size() - 2);
            lineIndices.push_back(lineVertices.size() - 1);

       }

       auto drawSphere(const glm::vec3 pos, const f32 radius) -> void {
        glm::mat4 modelMatrix = createModelMatrix(pos, glm::vec3{radius, radius, radius}, glm::quat(0.0f, 0.0f, 0.0f, 1.0));
            sphereInstanceData.emplace_back(
                        modelMatrix,
                        createNormalMatrix(modelMatrix)
                );
                
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

        Mesh &sphere = meshes[PrimitiveType::SPHERE];
        sphere.vertexBuffer = renderer->createVertexBuffer(sphere.getVertices().data(), sphere.getVertices().size() * sizeof(Vertex));
        sphere.indexBuffer = renderer->createIndexBuffer(sphere.getIndices().data(), sphere.getIndices().size() * sizeof(u16));

    }

    inline auto createModelMatrix(const glm::vec3 pos, const glm::vec3 scale, const glm::quat rot) -> glm::mat4 {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model *= glm::mat4_cast(rot);             // apply quaternion rotation
            model = glm::scale(model, scale);
            return model;
        }
    inline auto createNormalMatrix(const glm::mat4 model) -> std::array<glm::vec4, 3> 
    {
         auto normal =  glm::transpose(glm::inverse(glm::mat3(model)));
         return std::array<glm::vec4, 3> {
            glm::vec4(normal[0], 0.0f),
            glm::vec4(normal[1], 0.0f),
            glm::vec4(normal[2], 0.0f)


         };
    }



    VulkanRenderer *renderer;
    std::array<Mesh, static_cast<int>(PrimitiveType::CYLINDER) + 1> meshes;
    
    std::array<Buffer, static_cast<int>(PrimitiveType::CYLINDER)+1> instanceBuffers; 


    // Buffers for each frame in flight
    Buffer lightBuffer; 
    Buffer passBuffer; // UNUSED (for now aga)
    Buffer uniformBuffer;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    std::vector<LineVertex> lineVertices;
    std::vector<u16> lineIndices;    

    std::vector<InstanceData> sphereInstanceData;
  
};

}