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
    float3 worldPosition = mul(pc.model, float4(input.inPosition, 1.0)).xyz;
  //  float3 cameraRelativePos = worldPosition - ubo.cameraPosition.xyz;

    output.pos = mul(ubo.proj, mul(ubo.view, float4(worldPosition, 1.0)));
    output.uv = input.inTexCoords;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    const float line_width = 0.05f;
    float4 out_color = float4(0.5);
    const bool is_in_square =
        fract(vertIn.uv.x / 0.01) >= line_width &&
        fract(vertIn.uv.y / 0.01) >= line_width;
    if (is_in_square)
    {
        discard;
    }
    // Color the axes
    else if (!is_in_square && vertIn.uv.x > 0.5 && vertIn.uv.x < 0.5 + (line_width * 0.01))
    {
        out_color = float4(1.0, float3(0.0));
    }
    else if (!is_in_square && vertIn.uv.y > 0.5 && vertIn.uv.y < 0.5 + (line_width * 0.01))
    {
        out_color = float4(0.0, 1.0, 0.0, 0.0);
    }
    return out_color;
}