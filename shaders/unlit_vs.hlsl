cbuffer ConstantBuffer : register( b0 )
{
	matrix wvp : WorldViewProjection;
}

void main(float3 vertex : POSITION, out float4 position: SV_POSITION)
{ 
	position = mul(float4(vertex, 1.0), wvp);
}