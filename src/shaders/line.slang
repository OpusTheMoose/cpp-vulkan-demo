struct VSInput {
    float3 inPosition;
    float3 inColor;
    
};

struct UniformBuffer { 
    float4x4 view;
    float4x4 proj;
};

struct Instances {
    float4x4 model;
};


[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(0, 3)]]
StructuredBuffer<Instances> instanceModels;

struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos; 
    float3 color;
}


[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId: SV_InstanceID) {
  
    VSOutput output;

    float4x4 model = instanceModels[instanceId].model;
   

    output.pos = mul(ubo.proj, mul(ubo.view, mul(model, float4(input.inPosition, 1.0))));
    output.worldPos = mul(model, float4(input.inPosition, 1.0)).xyz;
    output.color = input.inColor;
    
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {

    return float4(vertIn.color, 1.0);
}