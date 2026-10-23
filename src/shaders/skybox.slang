struct VSInput {
    float3 inPosition;

};

struct UniformBuffer { 
    float4x4 view;
    float4x4 proj; 
};

[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(0, 2)]]
Sampler3D textureSampler;



struct VSOutput {
    float4 pos: SV_Position;

    float3 texCoords;
}


[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId: SV_InstanceID) {
  
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, float4(input.inPosition, 1.0)));
    output.texCoords = input.inPosition;


    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {


     


    return float4(textureSampler.Sample(vertIn.texCoords).xyz, 1.0);
}