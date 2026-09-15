struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoords;
    float3 inNormal;
};

struct UniformBuffer { 
    float4x4 view;
    float4x4 proj; 
};


[[vk::binding(0,0)]]
ConstantBuffer<UniformBuffer> ubo;

struct PushConstants{
    float4x4 model;
};

[[vk_push_constant]] 
PushConstants pc; 

struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos;
    float3 normal;
    float3 color;
}


[shader("vertex")]
VSOutput vertMain(VSInput input) {
  
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.inPosition, 1.0)))); 
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(1.0, 1.0, 1.0, 1.0);
}