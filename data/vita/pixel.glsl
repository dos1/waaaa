float4 main(
  uniform sampler2D al_tex,
  float4 varying_color : COLOR,
  float2 varying_texcoord : TEXCOORD0)
{
	float4 color = tex2D(al_tex, varying_texcoord);
	return color * varying_color;
}
