module;
#include "imgui.h"
#include "vulkan/vulkan.hpp"
#include <array>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/fwd.hpp>
#include <optional>
#include <variant>
#include <print>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/matrix.hpp>
#include <tracy/Tracy.hpp>


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
        QUAD = 4,
        CUSTOM = 5

};

export struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj; 
};

export struct Light {
   alignas(16) glm::vec3 pos;
   alignas(16) glm::vec3 color;
};



export class Draw3D {
    public:
        Draw3D(VulkanRenderer &renderer_) : renderer(renderer_), pipelineBuilder(renderer), texture("textures/2k_mercury.jpg") {

            for (auto &e : instanceBuffers)
            {
               e = renderer.createStorageBuffer(sizeof(InstanceData) * 1000);
            }

            renderer.uploadTextureVulkan(texture, vk::MemoryPropertyFlagBits::eDeviceLocal);

            lightBuffer = renderer.createStorageBuffer(sizeof(Light) * 50);
            uniformBuffer = renderer.createUniformBuffers(sizeof(UniformBufferObject));
            passBuffer = renderer.createStorageBuffer(8); 

            
            
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
            .setDepthWrite(true)
            .setStencilTest(vk::False)
            // Reasterizer
            .setDepthBiasEnable(vk::False)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setLineWidth(1.0f)
            .setDiscardEnable(vk::False)
            .setMultisampleSamples(vk::SampleCountFlagBits::e8)
            // Input Assembly
            .setInputAssembly(vk::PrimitiveTopology::eTriangleList)
            .setShader("src/shaders/instance.spv", vk::ShaderStageFlagBits::eVertex, "vertMain")
            .setShader("src/shaders/instance.spv", vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setVertexInputInfo(Vertex::getBindingDescription(),Vertex::getAttributeDescriptions())
            .setViewportState(1, 1)
            .setMultisampleShading(false)
            .build();

            // PipelineBuilder gridBuilder(renderer);
            // gridFloor = gridBuilder
            // .createFromExistingPipeline(pipelineBuilder)
            // .clearShaders()
            // .setShader("src/shaders/grid_floor.spv",
            //    vk::ShaderStageFlagBits::eVertex,
            //    "vertMain")
            // .setShader("src/shaders/grid_floor.spv",
            //    vk::ShaderStageFlagBits::eFragment,
            //    "fragMain") 
            // .setCullMode(vk::CullModeFlagBits::eNone)
            // .setDepthTest(true)
            // .setDepthWrite(true)
            // .setMultisampleSamples(vk::SampleCountFlagBits::e8)
            // .build();

            PipelineBuilder lineBuilder(renderer);
            linePipeline = lineBuilder
            .createFromExistingPipeline(pipelineBuilder)
            .clearShaders()
            .setShader("src/shaders/line.spv", vk::ShaderStageFlagBits::eVertex, "vertMain")
            .setShader("src/shaders/line.spv", vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setShader("src/shaders/line.spv", vk::ShaderStageFlagBits::eGeometry, "geomMain")
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setDepthTest(true)
            .setDepthWrite(true)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setInputAssembly(vk::PrimitiveTopology::eLineList)
            .setVertexInputInfo(LineVertex::getBindingDescription(),LineVertex::getAttributeDescriptions())
            .setMultisampleSamples(vk::SampleCountFlagBits::e8)
            .build();

            for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; ++frame) {
                renderer.uploadBuffer(TEMP_PIPELINE, uniformBuffer, vk::DescriptorType::eUniformBuffer, 0, 0, frame);
                
                renderer.uploadBuffer(TEMP_PIPELINE, lightBuffer, vk::DescriptorType::eStorageBuffer, 1, 0, frame);
               renderer.uploadBuffer(TEMP_PIPELINE, instanceBuffers[PrimitiveType::SPHERE], vk::DescriptorType::eStorageBuffer, 3, 0, frame);
                renderer.uploadImageBuffer(TEMP_PIPELINE, texture, vk::DescriptorType::eCombinedImageSampler, 2, 0, frame);

                renderer.uploadBuffer(linePipeline, uniformBuffer, vk::DescriptorType::eUniformBuffer, 0, 0, frame);
                
  
            }
            

        
            generateSphereData();
            generateQuadData();
            lineVertexBuffer = renderer.createDynamicVertexBuffer(sizeof(LineVertex) * 1000);


        }

        auto startDraw() {
            sphereInstanceData.clear();
            quadInstanceData.clear();
            lineVertices.clear();

        }
        // Submit for rendering
       auto endDraw(Camera camera) {

        Mesh& sphere = meshes[PrimitiveType::SPHERE];
        std::vector<Light> lights;
        lights.emplace_back(glm::vec3(1e8, 0.0, 0.0), 
        glm::vec3(1.0, 1.0, 1.0));
        
        

        // undo camera translations
        glm::mat4 view = camera.getViewMatrix();

        UniformBufferObject cameraUBO = {};
        cameraUBO.proj = camera.getProjMatrix();
        cameraUBO.view = view;

        for (u32 frame = 0; frame < FRAMES_IN_FLIGHT; ++frame) {
            ZoneScopedN("Uploading buffers for frame");
            renderer.setUniform(cameraUBO, uniformBuffer, frame);
            renderer.setStorageBuffer(instanceBuffers[PrimitiveType::SPHERE], std::span<const InstanceData>(sphereInstanceData), frame, instanceBuffers[PrimitiveType::SPHERE].bufferSize);
      
            renderer.setStorageBuffer(lightBuffer, std::span<const Light>(lights), frame, lightBuffer.bufferSize);

            const size_t bytes = lineVertices.size() * sizeof(LineVertex);
            if (bytes > lineVertexBuffer.bufferSize) {
                throw std::runtime_error("Dynamic vertex data exceeds buffer capacity");
                }

            std::memcpy(lineVertexBuffer.buffersMapped[frame],
                lineVertices.data(),
                    bytes);
        }
       
        drawCalls.emplace_back(
            TEMP_PIPELINE,
            sphere.vertexBuffer,
            &sphere.indexBuffer,
            sphereInstanceData, 
            std::nullopt,
            static_cast<u32>(sphere.getIndices().size()),
            static_cast<u32>(sphereInstanceData.size())
            
        );

  
        drawCalls.emplace_back(
            linePipeline,
            lineVertexBuffer,
            std::nullopt, 
            std::span<const InstanceData>(),
            PushConstant(camera.getViewport(), vk::ShaderStageFlagBits::eGeometry, 0),
            static_cast<u32>(lineVertices.size()),
            1
        );

     
    
        renderer.drawFrame(drawCalls, ImGui::GetDrawData());
        drawCalls.clear();
        
       }

       auto drawLine3D(const glm::vec3 start, const glm::vec3 end, const glm::vec3 color)
       {
            lineVertices.emplace_back(start, color);
            lineVertices.emplace_back(end, color);
        

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

       
       auto drawMesh(Mesh& mesh, GraphicsPipeline &pipeline, std::optional<PushConstant> pc)
       {

            drawCalls.emplace_back(
                pipeline,
                mesh.vertexBuffer,
                &mesh.indexBuffer,
                std::span<const InstanceData>(),
                pc,
                mesh.getIndices().size(),
                1
            );

       }

       template <typename T>
       auto setUniform(const T& data, const Buffer &buffer)
       {
            for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
            {
                renderer.setUniform(data, buffer, i);
            }
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
        quad.emplace_back(glm::vec3{1.0, 0.0, -1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 1.0}, glm::vec3{0.0, 1.0, 0.0} );
        // top right
        quad.emplace_back(glm::vec3{1.0, 0.0, 1.0}, glm::vec3{1.0, 1.0, 1.0}, glm::vec2{1.0, 0.0}, glm::vec3{0.0, 1.0, 0.0} );

        indices = {
            0, 2, 1, 0, 3, 2
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
    PipelineBuilder pipelineBuilder;
    GraphicsPipeline TEMP_PIPELINE; 
    GraphicsPipeline linePipeline;

    std::array<Mesh, static_cast<int>(PrimitiveType::QUAD) + 1> meshes;
    
    std::array<Buffer, static_cast<int>(PrimitiveType::QUAD)+1> instanceBuffers; 


    // Buffers for each frame in flight
    Buffer lightBuffer; 
    Buffer passBuffer; // UNUSED (for now aga)
    Buffer uniformBuffer;  
    
    //temp
    Buffer lineVertexBuffer;
    
    Texture texture; // temporary, for testing

    std::vector<LineVertex> lineVertices;
    std::vector<InstanceData> sphereInstanceData;
    std::vector<InstanceData> quadInstanceData;

    std::vector<DrawSubmission> drawCalls;
    
  
};

}