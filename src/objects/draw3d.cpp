module;
#include "vulkan/vulkan.hpp"
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
        CYLINDER = 3,
        QUAD = 4

};


export class Draw3D {
    public:
        Draw3D(VulkanRenderer &renderer_) : renderer(renderer_), pipelineBuilder(renderer) {

            for (auto &e : instanceBuffers)
            {
               e = renderer.createStorageBuffer(sizeof(InstanceData) * 1000);
            }

            lightBuffer = renderer.createStorageBuffer(sizeof(Light) * 50);
            // Create uniform buffers BEFORE descriptor sets!!
            uniformBuffer = renderer.createUniformBuffers(sizeof(UniformBufferObject));

            passBuffer = renderer.createStorageBuffer(8); // just pass some bs

            // Per frame
            this->descriptorSets.layouts[0] = renderer.createDescriptorSetLayout(vk::ShaderStageFlagBits::eVertex, vk::DescriptorType::eUniformBuffer);
            this->descriptorSets.layouts[1] = renderer.createDescriptorSetLayout(vk::ShaderStageFlagBits::eVertex, vk::DescriptorType::eStorageBuffer);
            // per material
            this->descriptorSets.layouts[2] = renderer.createDescriptorSetLayout(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, vk::DescriptorType::eStorageBuffer);
            // Per object
            this->descriptorSets.layouts[3] = renderer.createDescriptorSetLayout(vk::ShaderStageFlagBits::eVertex, vk::DescriptorType::eStorageBuffer);


            this->descriptorSets.frameSets = renderer.allocateDescriptorSets(descriptorSets.layouts[0], FRAMES_IN_FLIGHT);
            this->descriptorSets.passSets = renderer.allocateDescriptorSets(descriptorSets.layouts[1], FRAMES_IN_FLIGHT);
            this->descriptorSets.materialSets = renderer.allocateDescriptorSets(descriptorSets.layouts[2], FRAMES_IN_FLIGHT);
            this->descriptorSets.instanceSets = renderer.allocateDescriptorSets(descriptorSets.layouts[3], FRAMES_IN_FLIGHT);

            for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; ++frame) {
                renderer.updateDescriptorSet( FRAME, descriptorSets.frameSets[frame], uniformBuffer, frame);
                renderer.updateDescriptorSet( MATERIAL, descriptorSets.materialSets[frame], lightBuffer, frame);
                renderer.updateDescriptorSet( OBJECT,   descriptorSets.instanceSets[frame],    instanceBuffers[PrimitiveType::SPHERE], frame);  
            }
            const std::array<vk::DescriptorSetLayout, 4> pipelineSetLayouts {
                *this->descriptorSets.layouts[0],
                *this->descriptorSets.layouts[1],
                *this->descriptorSets.layouts[2],
                *this->descriptorSets.layouts[3]
            };
            
            TEMP_PIPELINE = pipelineBuilder
            .setColorAttachmentFormats(std::array{renderer.getSwapSurfaceFormat().format})
            .setRenderingColorAttachmentCount(1)
            .setColorBlendAttachment(false, vk::ColorComponentFlagBits::eR | 
                                           vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB |
                                           vk::ColorComponentFlagBits::eA
                                        
                                        )
            .setColorBlendState()
            //Depth
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
            .setDepthClampEnable(vk::False)
            .setDepthTest(vk::True)
            .setStencilTest(vk::False)
            // Reasterizer
            .setDepthBiasEnable(vk::False)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setLineWidth(1.0f)
            .setDiscardEnable(vk::False)
            // Input Assembly
            .setInputAssembly(vk::PrimitiveTopology::eTriangleList)
            .setShader("src/shaders/instance.spv", vk::ShaderStageFlagBits::eVertex, "vertMain")
            .setShader("src/shaders/instance.spv", vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setVertexInputInfo(Vertex::getBindingDescription(),Vertex::getAttributeDescriptions())
            .setViewportState(1, 1)
            .setMultisampleSamples(vk::SampleCountFlagBits::e1) 
            .setMultisampleShading(false)
            .setPipelineSetLayouts(pipelineSetLayouts)
            .build();

            PipelineBuilder gridBuilder(renderer);
            gridFloor = gridBuilder
            .createFromExistingPipeline(pipelineBuilder)
            .clearShaders()
            .setShader("src/shaders/grid_floor.spv",
               vk::ShaderStageFlagBits::eVertex,
               "vertMain")
            .setShader("src/shaders/grid_floor.spv",
               vk::ShaderStageFlagBits::eFragment,
               "fragMain")
            .build();
        
            generateSphereData();
            generateQuadData();
    
        }

        auto startDraw() {
            sphereInstanceData.clear();
            quadInstanceData.clear();
            lineVertices.clear();
            lineIndices.clear();

        }
        // Submit for rendering
       auto endDraw(Camera camera) {

        Mesh& sphere = meshes[PrimitiveType::SPHERE];
        std::vector<Light> lights;
        lights.emplace_back(glm::vec3(0.0, -5.0, 0.0), glm::vec3(0.8));
        
        std::vector<DrawSubmission> drawCalls;
       
        drawCalls.emplace_back(
            TEMP_PIPELINE,
            sphere.vertexBuffer,
            sphere.indexBuffer,
            uniformBuffer,
            instanceBuffers[PrimitiveType::SPHERE],
            lightBuffer,
            sphereInstanceData,
            lights,
            descriptorSets,
            static_cast<u32>(sphere.getIndices().size()),
            static_cast<u32>(sphereInstanceData.size())
        );

        Mesh& quad = meshes[PrimitiveType::QUAD];
         drawCalls.emplace_back(
            gridFloor,
            quad.vertexBuffer,
            quad.indexBuffer,
            uniformBuffer,
            instanceBuffers[PrimitiveType::QUAD],
            lightBuffer,
            quadInstanceData,
            lights,
            descriptorSets,
            static_cast<u32>(quad.getIndices().size()),
            static_cast<u32>(quadInstanceData.size())
        );

    
        renderer.drawFrame(camera, drawCalls);
        
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
        auto drawQuad(const glm::vec3 pos, const glm::vec3 scale) -> void {
        glm::mat4 modelMatrix = createModelMatrix(pos, scale, glm::quat(0.0f, 0.0f, 0.0f, 1.0));
            quadInstanceData.emplace_back(
                        modelMatrix,
                        createNormalMatrix(modelMatrix)
                );
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
        sphere.vertexBuffer = renderer.createVertexBuffer(sphere.getVertices().data(), sphere.getVertices().size() * sizeof(Vertex));
        sphere.indexBuffer = renderer.createIndexBuffer(sphere.getIndices().data(), sphere.getIndices().size() * sizeof(u16));

    }
    
    auto generateQuadData() -> void {
        std::vector<Vertex> quad;
        std::vector<u16> indices;
        // top left
        quad.emplace_back(glm::vec3{-1.0, 0.0, 1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{0.0, 0.0}, glm::vec3{0.0, 1.0, 0.0} );
        // bottom left
        quad.emplace_back(glm::vec3{-1.0, 0.0, -1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{0.0, 1.0}, glm::vec3{0.0, 1.0, 0.0} );
        // bottom right
        quad.emplace_back(glm::vec3{1.0, 0.0, -1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 0.0}, glm::vec3{0.0, 1.0, 0.0} );
        // top right
        quad.emplace_back(glm::vec3{1.0, 0.0, 1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 1.0}, glm::vec3{0.0, 1.0, 0.0} );

        indices = {
            0, 1, 2, 0, 2, 3
        };
        meshes[PrimitiveType::QUAD] = Mesh(quad, indices);
        Mesh &quadMesh = meshes[PrimitiveType::QUAD];
        quadMesh.vertexBuffer = renderer.createVertexBuffer(quadMesh.getVertices().data(), quadMesh.getVertices().size() * sizeof(Vertex));
        quadMesh.indexBuffer = renderer.createIndexBuffer(quadMesh.getIndices().data(), quadMesh.getIndices().size() * sizeof(u16));



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



    VulkanRenderer& renderer;
    Descriptors descriptorSets;
    PipelineBuilder pipelineBuilder;
    GraphicsPipeline TEMP_PIPELINE;
    GraphicsPipeline gridFloor;

    std::array<Mesh, static_cast<int>(PrimitiveType::QUAD) + 1> meshes;
    
    std::array<Buffer, static_cast<int>(PrimitiveType::QUAD)+1> instanceBuffers; 


    // Buffers for each frame in flight
    Buffer lightBuffer; 
    Buffer passBuffer; // UNUSED (for now aga)
    Buffer uniformBuffer;
    

    std::vector<LineVertex> lineVertices;
    std::vector<u16> lineIndices;    

    std::vector<InstanceData> sphereInstanceData;
    std::vector<InstanceData> quadInstanceData;
  
};

}