struct VSInput {
    float3 inPosition;
    float3 inColor;
    
};

struct UniformBuffer { 
    float4x4 view;
    float4x4 proj;
};


struct PushConstants{
    float2 viewport;
};

[[vk_push_constant]] 
PushConstants pc; 


[[vk::binding(0,0 )]]
ConstantBuffer<UniformBuffer> ubo;


struct VSOutput {
    float4 pos: SV_Position;
    float3 worldPos;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId: SV_InstanceID) {
  
    VSOutput output;

    float4x4 model = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };  
   

    output.pos = mul(ubo.proj, mul(ubo.view, mul(model, float4(input.inPosition, 1.0))));
    output.worldPos = mul(model, float4(input.inPosition, 1.0)).xyz;
    output.color = input.inColor;
    
    return output;
}


[shader("geometry")]
[maxvertexcount(4)]
void geomMain(line VSOutput inputVertices[2],
                    inout TriangleStream<VSOutput> outputStream) {
        float4 p0 = inputVertices[0].pos;
        float4 p1 = inputVertices[1].pos;

        // Convert to NDC, then measure direction in pixels so the
        // perpendicular offset works correctly for a non-square viewport.
        float2 ndc0 = p0.xy / p0.w;
        float2 ndc1 = p1.xy / p1.w;
        float2 directionPixels = (ndc1 - ndc0) * pc.viewport;

        float lengthSquared = dot(directionPixels, directionPixels);
        if (lengthSquared < 1e-6)
            return;

        float2 normalPixels =
        normalize(float2(-directionPixels.y, directionPixels.x));

        const float maxLineWidth = 0.5;
        // Convert half the requested pixel width back to NDC.
        float2 offsetNdc = normalPixels * (maxLineWidth / pc.viewport);

        VSOutput v0 = inputVertices[0];
        VSOutput v1 = inputVertices[0];
        VSOutput v2 = inputVertices[1];
        VSOutput v3 = inputVertices[1];

        v0.pos.xy -= offsetNdc * p0.w;
        v1.pos.xy += offsetNdc * p0.w;
        v2.pos.xy -= offsetNdc * p1.w;
         v3.pos.xy += offsetNdc * p1.w;

        outputStream.Append(v0);
        outputStream.Append(v1);
        outputStream.Append(v2);
        outputStream.Append(v3);
        outputStream.RestartStrip();
}


[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {

    return float4(vertIn.color, 1.0);
}
