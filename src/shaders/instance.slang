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

struct Light {
    float3 pos;
    float3 color;
}

[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(3, 0)]]
StructuredBuffer<float4x4> instanceModels;

[[vk::binding(2, 0)]]
StructuredBuffer<Light> lights;

struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos;
    float3 normal;
    float3 color;
}


[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId: SV_InstanceID) {
  
    VSOutput output;

    float4x4 model = instanceModels[instanceId];

    output.pos = mul(ubo.proj, mul(ubo.view, mul(model, float4(input.inPosition, 1.0))));
    output.worldPos = mul(model, float4(input.inPosition, 1.0)).xyz;
    output.color = input.inColor;
    output.normal =  input.inNormal;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 norm = normalize(vertIn.normal);
    // Just 1 light for now
    float3 lightDir = normalize(lights[0].pos - vertIn.worldPos );
    float diff = max(dot(norm, lightDir), 0.0);
    float3 diffuse = diff * lights[0].color;

    float3 ambient = float3(0.2, 0.2, 0.2);

    float3 result = (ambient + diffuse) * vertIn.color;


    return float4(result, 1.0);
}