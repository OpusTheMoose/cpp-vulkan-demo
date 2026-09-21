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
    float4 pos : SV_Position;
    float2 uv;
}


[shader("vertex")]
VSOutput vertMain(VSInput input) {
  
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.inPosition, 1.0))));
    output.uv = input.inTexCoords;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    const float line_width = 0.01f;
    if (fract(vertIn.uv.x / 0.01) >= line_width &&
    fract(vertIn.uv.y / 0.01) >= line_width)
    {
        discard;
    }
    return float4(1.0);
}