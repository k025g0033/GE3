#define float32_t4 float4
#define float32_t2 float2

struct VertexShaderOutput
{
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
};