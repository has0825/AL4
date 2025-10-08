
#include "object3d.hlsli"


ConstantBuffer<Material> gMaterial : register(b0);


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;

    //これは不要同じスコープで二回宣言するとエラーになるからねー06_01
    //float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // UV座標を同次座標系に拡張して（x, y, 1.0）、アフィン変換を適用する
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // 変換後のUV座標を使ってテクスチャから色をサンプリングする
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        
    
    if (gMaterial.enableLighting != 0)//Lightingする場合
    {
        //float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        //output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
        //half lambert
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        output.color = cos * gMaterial.color * textureColor;
        
        
    }
    else
    { //Lightingしない場合前回までと同じ計算
        output.color = gMaterial.color * textureColor;
    }
    
    
    return output;
}