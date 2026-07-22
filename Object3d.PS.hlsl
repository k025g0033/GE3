#include "object3d.hlsli"
#define float32_t4 float4
#define float32_t3 float3
#define float32_t2 float2
#define int32_t int

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;
    
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    if (gMaterial.enableLighting != 0)
    {
        float cosValue = saturate(
            dot(
                 normalize(input.normal),
        normalize(-gDirectionalLight.direction)
            )
        );

        output.color =
            gMaterial.color *
            textureColor *
            gDirectionalLight.color *
            cosValue *
            gDirectionalLight.intensity;
    }
    else
    {
        output.color =
            gMaterial.color * textureColor;
    }

    return output;
}