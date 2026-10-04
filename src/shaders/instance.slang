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
};

struct Instances {
    float4x4 model;
    float4 normal[3];
};

[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(0, 1)]]
StructuredBuffer<Light> lights;

[[vk::binding(0, 2)]]
Sampler2D textureSampler;

[[vk::binding(0, 3)]]
StructuredBuffer<Instances> instanceModels;

struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos;
    float3 normal;
    float3 color;
    float2 texCoords;
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

    float3 worldPosition = mul(model, float4(input.inPosition, 1.0)).xyz;

    output.pos = mul(ubo.proj, mul(ubo.view, float4(worldPosition, 1.0)));
    output.worldPos = worldPosition;
    output.color = input.inColor;
    output.normal =  mul(normalMatrix, input.inNormal);
    output.texCoords = input.inTexCoords;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 norm = normalize(vertIn.normal);
    // Just 1 light for now
    float3 lightDir = normalize(lights[0].pos - vertIn.worldPos );
    float diff = max(dot(norm, lightDir), 0.0);
    float3 diffuse = diff * lights[0].color;

    float3 ambient = float3(0.1, 0.1, 0.1);

    // specular
    float3 viewDir = normalize(-vertIn.worldPos);
    float3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    float3 specular = 0.5 * spec * lights[0].color;


    float3 result = (ambient + diffuse + specular) * textureSampler.Sample(vertIn.texCoords).xyz;


    return float4(result, 1.0);
}