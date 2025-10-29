
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

    //����͕s�v�����X�R�[�v�œ��錾����ƃG���[�ɂȂ邩��ˁ[06_01
    //float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // UV���W�𓯎����W�n�Ɋg�����āix, y, 1.0�j�A�A�t�B���ϊ���K�p����
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // �ϊ����UV���W��g���ăe�N�X�`������F��T���v�����O����
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        
    
    if (gMaterial.enableLighting != 0)//Lighting����ꍇ
    {
        //float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        //output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
        //half lambert
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        output.color = cos * gMaterial.color * textureColor;
        
        
    }
    else
    { //Lighting���Ȃ��ꍇ�O��܂łƓ����v�Z
        output.color = gMaterial.color * textureColor;
    }
    
    
    return output;
}