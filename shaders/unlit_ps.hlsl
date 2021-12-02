cbuffer ConstantBuffer : register( b0 )
{
	float4 diffuseMaterial = float4(0.8f, 0.2f, 0.0f, 0.0f);
}

float4 main() : SV_Target
{
	float4 color = diffuseMaterial;
	return color;
}
