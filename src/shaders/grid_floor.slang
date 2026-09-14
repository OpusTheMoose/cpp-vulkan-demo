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



struct Instances {
    float4x4 model;
    float4 normal[3];
};

[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;



[[vk::binding(3, 0)]]
StructuredBuffer<Instances> instanceModels;

struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos;
    float3 normal;
    float3 color;
}


[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId: SV_InstanceID) {
  
    VSOutput output;

    float4x4 model = instanceModels[instanceId].model;
    float3x3 normalMatrix = float3x3(
        instanceModels[instanceId].normal[0].xyz,
        instanceModels[instanceId].normal[1].xyz,
        instanceModels[instanceId].normal[2].xyz
    );

    output.pos = mul(ubo.proj, mul(ubo.view, mul(model, float4(input.inPosition, 1.0))));
    output.worldPos = mul(model, float4(input.inPosition, 1.0)).xyz;
    output.color = input.inColor;
    output.normal =  mul(normalMatrix, input.inNormal);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(1.0, 1.0, 1.0, 1.0);
}