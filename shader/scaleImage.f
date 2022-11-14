#version 330

precision highp float;

//Floating-point samplers begin with "sampler". 
//Signed integer samplers begin with "isampler".
//Unsigned integer samplers begin with "usampler".
uniform sampler2D sourceTexture;

uniform bool isNearestNeighbor;

//Dimension for the currently bound framebuffer.
uniform uint fbWidth;
uniform uint fbHeight;

in vec4 gl_FragCoord;

in vec2 uv;

out vec3 color;

vec3 nearestNeighbor()
{
  //gl_FragCoord : [0.5, dimension - 1 + 0.5]

  //textureCoordinates : [1, fbDimension]
  vec2 textureCoordinates = ceil(gl_FragCoord.xy);

  vec2 ratio = textureSize(sourceTexture, 0);
  ratio.x /= fbWidth;
  ratio.y /= fbHeight;

  //textureCoordinates : [1, sourceDimension]
  textureCoordinates *= ratio;

  //textureCoordinates : [0, sourceDimension - 1]; texelFetch()
  textureCoordinates -= 1;

  //Our ppm files are stripped of 4th alpha component.
  return texelFetch(sourceTexture, ivec2(textureCoordinates), 0).xyz;
}

vec3 bilinear()
{
  //gl_FragCoord : [0.5, dimension - 1 + 0.5]

  //Set rules on what texels to fetch for our own
  //"top-left", "top-right", "bottom-left", "bottom-right" pixels.
  //
  // e.g.) 4 x 4
  // gl_FragCoord basis
  //
  // (-0.5, -0.5) (0.5, -0.5)     (2.5, -0.5) (3.5, -0.5)
  // (-0.5,  0.5) (0.5,  0.5) ... (2.5,  0.5) (3.5,  0.5)
  // similar to above, but in texelFetch() basis
  // (-1.0, -1.0) (0.0, -1.0)     (2.0, -1.0) (3.0, -1.0)
  // (-1.0,  0.0) (0.0,  0.0) ... (2.0,  0.0) (3.0,  0.0)

  //Calculate the relative position of the current pixel;
  //map the current pixel to the source texture's dimensions, basis, ...
  //
  // e.g.) (0.5, 0.5) in 4 x 4 texture, and map it to 2 x 2 texture
  // 0.5 / 4 = 1 / 8 (ratio in both x and y directions)
  // 2 * 1 / 8 = 1 / 4 -> (0.25, 0.25)

  vec2 texels = gl_FragCoord.xy;
  texels /= vec2(fbWidth, fbHeight); 
  texels *= textureSize(sourceTexture, 0);

  //Set rules on what texels to fetch for our own
  //"top-left", "top-right", "bottom-left", "bottom-right" pixels.
  // Possible mapped values in the above scenario are
  // (0.25, 0.25) (0.75, 0.25), (1.25, 0.25), ...
  // 
  // e.g.) Find 4 corners of (0.25, 0.25), and the current pixel always reside in "top-right"
  // "top-right"    =             floor(x),             floor(y); (0.0, 0.0)
  // "top-left"     = max(floor(x - 1), 0),             floor(y); (0.0, 0.0)
  // "bottom-right" =             floor(x), max(floor(y - 1), 0); (0.0, 0.0)
  // "bottom-left"  = max(floor(x - 1), 0), max(floor(y - 1), 0); (0.0, 0.0)
  //
  // e.g.) Find 4 corners of (1.75, 1.75), and the current pixel always reside in "top-right"
  // "top-right"    =             floor(x),             floor(y); (1.0, 1.0)
  // "top-left"     = max(floor(x - 1), 0),             floor(y); (0.0, 1.0)
  // "bottom-right" =             floor(x), max(floor(y - 1), 0); (1.0, 0.0)
  // "bottom-left"  = max(floor(x - 1), 0), max(floor(y - 1), 0); (0.0, 0.0)

  // e.g.) Find 4 corners of (1.75, 0.25), and the current pixel always reside in "top-right"
  // "top-right"    =             floor(x),             floor(y); (1.0, 0.0)
  // "top-left"     = max(floor(x - 1), 0),             floor(y); (0.0, 0.0)
  // "bottom-right" =             floor(x), max(floor(y - 1), 0); (1.0, 0.0)
  // "bottom-left"  = max(floor(x - 1), 0), max(floor(y - 1), 0); (0.0, 0.0)

  // e.g.) Find 4 corners of (0.25, 1.75), and the current pixel always reside in "top-right"
  // "top-right"    =             floor(x),             floor(y); (0.0, 1.0)
  // "top-left"     = max(floor(x - 1), 0),             floor(y); (0.0, 1.0)
  // "bottom-right" =             floor(x), max(floor(y - 1), 0); (0.0, 0.0)
  // "bottom-left"  = max(floor(x - 1), 0), max(floor(y - 1), 0); (0.0, 0.0)

  // "bottom-left" = ceil(x - 1, y) = (0.0, 0.0); ceil(0.25 - 1) = 0, ceil(0.25) = 1

  ivec2 topRight    = ivec2( floor( texels ) );
  ivec2 topLeft     = ivec2( max( floor( texels.x - 1.0 ), 0.0 ), floor( texels.y ) );
  ivec2 bottomRight = ivec2( floor( texels.x ), max( floor( texels.y - 1.0 ), 0.0 ) );
  ivec2 bottomLeft  = ivec2( max( floor( texels.x - 1.0 ), 0.0 ), max( floor( texels.y - 1.0 ), 0.0 ) );

  vec3 top = mix(texelFetch(sourceTexture, topLeft, 0), texelFetch(sourceTexture, topRight, 0), (texels.x - topRight.x + 0.5) / 2.0).xyz;
  vec3 bottom = mix(texelFetch(sourceTexture, bottomLeft, 0), texelFetch(sourceTexture, bottomRight, 0), (texels.x - topRight.x + 0.5) / 2.0).xyz;
  return mix(bottom, top, (texels.y - topRight.y + 0.5) / 2.0).xyz;
}

void main()
{
  if (isNearestNeighbor)
    color = nearestNeighbor();
  else
    color = bilinear();
}