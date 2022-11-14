#version 330

//model to world
uniform mat4 mtow;
//world to camera
uniform mat4 wtoc;
//camera to clip
uniform mat4 ctoc;

out vec2 uv;

layout(location = 0) in vec3 vertexPosition;

void main()
{
  uv = vertexPosition.xy;
  gl_Position = (ctoc * wtoc * mtow * vec4(vertexPosition.xyz, 1.f));
}