void main(
  float4 al_pos : POSITION,
  float4 al_color : COLOR,
  float2 al_texcoord : TEXCOORD0,
  uniform float4x4 al_projview_matrix,
  uniform bool al_use_tex_matrix,
  uniform float4x4 al_tex_matrix,
  float4 out varying_color : COLOR,
  float2 out varying_texcoord : TEXCOORD0,
  float4 out gl_Position : POSITION)
{
  varying_color = al_color;
  if (al_use_tex_matrix) {
    float4 uv = mul(float4(al_texcoord, 0, 1), al_tex_matrix);
    varying_texcoord = float2(uv.x, uv.y);
  }
  else
    varying_texcoord = al_texcoord;
  gl_Position = mul(al_pos, al_projview_matrix);
}
