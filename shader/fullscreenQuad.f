#version 330

precision highp float;

//Floating-point samplers begin with "sampler". 
//Signed integer samplers begin with "isampler".
//Unsigned integer samplers begin with "usampler".
uniform sampler2D sourceTexture;

in vec2 uv;

out vec3 color;

void main()
{
  color = texture(sourceTexture, vec2(uv.x, 1.0 - uv.y)).xyz;
  //color = texelFetch(sourceTexture, ivec2(gl_FragCoord.xy - 0.5), 0).xyz;
}