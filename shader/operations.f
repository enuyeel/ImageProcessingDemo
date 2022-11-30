#version 330

#define PI 3.14159265359

precision highp float;

//Floating-point samplers begin with "sampler". 
//Signed integer samplers begin with "isampler".
//Unsigned integer samplers begin with "usampler".
uniform sampler2D sourceTexture1;
uniform sampler2D sourceTexture2;

uniform int operationIndex;

////////////////////////
//Gaussian Blur/////////
uniform uint kernelSize;
uniform float sigma;
////////////////////////

////////////////////////
//Unsharp Mask//////////
uniform float k;
////////////////////////

in vec4 gl_FragCoord;

out vec3 color;

float luminance(in vec3 rgb)
{
  //[http://www.songho.ca/dsp/luminance/luminance.html]
  //RGB to Grayscale
  //In order to convert RGB or BGR color image to grayscale image, we are frequently use the following conversion formulae :
  //Luminace = 0.3086 * Red + 0.6094 * Green + 0.0820 * Blue
  //Luminace = 0.299 * Red + 0.587 * Green + 0.114 * Blue
  //uint8_t luminance = 0.3086f * buffer[bufferIdx] + 0.6094f * buffer[bufferIdx + 1] + 0.0820f * buffer[bufferIdx + 2];
  return dot(rgb, vec3(0.3086f, 0.6094f, 0.082f));
}

vec3 sobel()
{
  //[https://www.shadertoy.com/view/Xdf3Rf]

  vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
  vec2 p = floor(gl_FragCoord.xy) / textureDimension;
  vec2 pixelOffset = 1.f / textureDimension;

  float tl = luminance(texture(sourceTexture1, p + vec2(-pixelOffset.x,  pixelOffset.y), 0).rgb);
  float tc = luminance(texture(sourceTexture1, p + vec2(             0,  pixelOffset.y), 0).rgb);
  float tr = luminance(texture(sourceTexture1, p + vec2( pixelOffset.x,  pixelOffset.y), 0).rgb);
  float  l = luminance(texture(sourceTexture1, p + vec2(-pixelOffset.x,              0), 0).rgb);
  float  r = luminance(texture(sourceTexture1, p + vec2( pixelOffset.x,              0), 0).rgb);
  float bl = luminance(texture(sourceTexture1, p + vec2(-pixelOffset.x, -pixelOffset.y), 0).rgb);
  float bc = luminance(texture(sourceTexture1, p + vec2(             0, -pixelOffset.y), 0).rgb);
  float br = luminance(texture(sourceTexture1, p + vec2( pixelOffset.x, -pixelOffset.y), 0).rgb);

  // | -1  0  1 |    | 1 0 -1 |
  // | -2  0  2 | -> | 2 0 -2 |
  // | -1  0  1 |    | 1 0 -1 |
  float x =   tl -tr 
            + 2.f * l -2.f * r 
            + bl -br;

  // |  1  2  1 |
  // |  0  0  0 |
  // | -1 -2 -1 |
  float y =  tl + 2.f * tc + tr
            -bl  -2.f * bc  -br;

  return texture(sourceTexture1, p).rgb*sqrt(x*x + y*y);
}

vec3 imageNegative()
{
  return 1.0 - texelFetch(sourceTexture1, ivec2(gl_FragCoord.xy), 0).rgb;
}

vec3 chromaticAberration()
{
    vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
    vec2 p = floor(gl_FragCoord.xy) / textureDimension;
	float amount = 0.125;
	
	//amount = (1.0 + sin(iTime*6.0)) * 0.5;
	//amount *= 1.0 + sin(iTime*16.0) * 0.5;
	//amount *= 1.0 + sin(iTime*19.0) * 0.5;
	//amount *= 1.0 + sin(iTime*27.0) * 0.5;
	//amount = pow(amount, 3.0);

	//amount *= 0.05;
	
    vec3 col;
    col.r = texture( sourceTexture1, vec2(p.x+amount,p.y) ).r;
    col.g = texture( sourceTexture1, p ).g;
    col.b = texture( sourceTexture1, vec2(p.x-amount,p.y) ).b;

	col *= (1.0 - amount * 0.5);
	
    return col;
}

vec3 pixellating()
{
    float pixelly = 0.1f;

    vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
    vec2 uv = floor(gl_FragCoord.xy) / textureDimension;
    uv = floor(uv*textureDimension.x*pixelly)/(textureDimension.x*pixelly);

    return texture(sourceTexture1, uv).rgb;
}

vec3 waterColor()
{
    vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
    vec2 blend_uv = floor(gl_FragCoord.xy) / textureDimension;
    vec2 uv = vec2(1.0 - blend_uv.x, blend_uv.y);
    vec3 intensity = 1.0 - texture(sourceTexture1, uv).rgb;
    
    float vidSample = dot(vec3(1.0), texture(sourceTexture1, uv).rgb);
    float delta = 0.005;
    float vidSampleDx = dot(vec3(1.0), texture(sourceTexture1, uv + vec2(delta, 0.0)).rgb);
    float vidSampleDy = dot(vec3(1.0), texture(sourceTexture1, uv + vec2(0.0, delta)).rgb);
    
    vec2 flow = delta * vec2 (vidSampleDy - vidSample, vidSample - vidSampleDx);
    
    intensity = 0.005 * intensity + 0.995 * (1.0 - texture(sourceTexture2, blend_uv + vec2(-1.0, 1.0) * flow).rgb);
    return 1.0 - intensity;
}

vec3 radialFlare()
{
    float imageBrightness = 9.0;
    float flareBrightness = 4.5;
    float radialLength = 0.95;

    vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
    vec3 p = vec3(gl_FragCoord.xy / textureDimension, max(0.0, (imageBrightness/10.0)-0.5)) - 0.5;
    vec3 o = texture(sourceTexture1,0.5+(p.xy*=0.992)).rgb;
    
    for (float i=0.0; i<100.0; i++)
    {
        p.z += pow(max(0.0, 0.5-length(o)), 10.0/flareBrightness) * exp(-i * (1.0-(radialLength)) );
    }
    
    vec3 flare = p.z * vec3(0.7, 0.9, 1.0); //tint
    
    return o*o+flare;
}

vec3 Meshify()
{
    color -= color;
    vec2 uv = gl_FragCoord.xy;
    uv /= 8.0;
    vec2  p = floor(uv+.5);
    vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);

    #define T(x,y) texture(sourceTexture1,8.0*vec2(x,y)/textureDimension.xy).g   // add .g or nothing 

    #define M(c,T) color += pow(.5+.5*cos( 6.28*(uv-p).c + 4.0*(2.*T-1.) ),6.0)

    M( y, T( uv.x, p.y ) );   // modulates  y offset
    M( x, T( p.x, uv.y ) );   // modulates  y offset

    return color;
}

void main()
{
  switch (operationIndex)
  {
    case 0:
      ivec2 p = ivec2(gl_FragCoord.xy - 0.5);
      color = texelFetch(sourceTexture1, p, 0).rgb;
      break;
    
    case 1:
      color = imageNegative();
      break;

    case 2:
      color = sobel();
      break;

    case 3:
      color = chromaticAberration();
      break;

    case 4:
      color = pixellating();
      break;

    case 5:
      color = waterColor();
      break;

    case 6:
      color = Meshify();
      break;

//    default:
//      break;
  }
}