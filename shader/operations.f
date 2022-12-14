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

uniform float iTime;

////////////////////////
/////Gradient Color/////
uniform uint paletteIdx;
////////////////////////

////////////////////////
//Chromatic Aberration//
uniform float offsetAmount;
uniform uint chromaticAuto;
////////////////////////

///////////////////////
////////Meshify////////
uniform float interlineDistance;
uniform float amplicationFactor;
uniform float meshifyThickness;
uniform uint meshify;
///////////////////////


/////////////////////
///////Sketch////////
uniform float angleNum;
uniform float range;
uniform float sensitivity;

in vec4 gl_FragCoord;

out vec3 color;

#define PI2 6.28318530717959

#define RANGE 16.
#define STEP 2.
#define MAGIC_GRAD_THRESH 0.01
#define MAGIC_COLOR           0.5

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

vec4 getCol(vec2 pos)
{
    vec2 uv = pos /textureSize(sourceTexture1, 0).xy;
    return texture(sourceTexture1, uv);
}

vec2 getGrad(vec2 pos, float eps)
{
    vec2 textureDimension = textureSize(sourceTexture1, 0);
   	vec2 d=vec2(eps,0);
    return vec2(
        luminance(texture(sourceTexture1,(pos+d.xy)/textureDimension.xy).rgb)-luminance(texture(sourceTexture1,(pos-d.xy)/textureDimension.xy).rgb),
        luminance(texture(sourceTexture1,(pos+d.yx)/textureDimension.xy).rgb)-luminance(texture(sourceTexture1,(pos-d.yx)/textureDimension.xy).rgb)
    )/eps/2.;
}

void pR(inout vec2 p, float a) 
{
	p = cos(a)*p + sin(a)*vec2(p.y, -p.x);
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
	
  float amount = offsetAmount;
  if (chromaticAuto == uint(1))
  {
	  amount = (1.0 + sin(iTime*6.0)) * 0.5;
	  amount *= 1.0 + sin(iTime*16.0) * 0.5;
	  amount *= 1.0 + sin(iTime*19.0) * 0.5;
	  amount *= 1.0 + sin(iTime*27.0) * 0.5;
	  amount = pow(amount, 3.0);
    amount *= 0.05;
  }
	
  vec3 col;
  col.r = texture( sourceTexture1, vec2(p.x + amount, p.y) ).r;
  col.g = texture( sourceTexture1, p ).g;
  col.b = texture( sourceTexture1, vec2(p.x - amount, p.y) ).b;

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
    vec2 blend_uv = gl_FragCoord.xy / textureSize(sourceTexture1, 0);
    vec2 uv = vec2(1.0-blend_uv.x, blend_uv.y);
    vec3 intensity = 1.0 - texture(sourceTexture1, uv).rgb;
    
  float vidSample = dot(vec3(1.0), texture(sourceTexture1, uv).rgb);
  float delta = 0.005;
  float vidSampleDx = dot(vec3(1.0), texture(sourceTexture1, uv + vec2(delta, 0.0)).rgb);
  float vidSampleDy = dot(vec3(1.0), texture(sourceTexture1, uv + vec2(0.0, delta)).rgb);
    
  vec2 flow = delta * vec2 (vidSampleDy - vidSample, vidSample - vidSampleDx);
    
    intensity = 0.0225 * intensity + 0.9775 * (1.0 - texture(sourceTexture2, blend_uv + vec2(-1.0, 1.0) * flow).rgb); //blend_uv).rgb);
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

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}

vec3 GradientColor()
{
  vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);
  vec2 uv = floor(gl_FragCoord.xy) / textureDimension;

  float lum = luminance(texture(sourceTexture1, uv, 0).rgb);

  switch (paletteIdx)
  {
    case uint(0):
    {
      vec3 heat;      
      heat.r = smoothstep(0.5, 0.8, lum);
  
      if(lum >= 0.90)
        heat.r *= (1.1 - lum) * 5.0;

	    if(lum > 0.7) 
		    heat.g = smoothstep(1.0, 0.7, lum);
      else 
		    heat.g = smoothstep(0.0, 0.7, lum);

	    heat.b = smoothstep(1.0, 0.0, lum);          
      if(lum <= 0.3) 
        heat.b *= lum / 0.3;     

	    return heat;
    } break;

    case uint(1):
    {
      return pal(lum,vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(0.8,0.8,0.8),vec3(0.0,0.33,0.67)+0.21);
    } break;

    case uint(2):
    {
      return pal(lum,vec3(0.55,0.4,0.3),vec3(0.50,0.51,0.35)+0.1,vec3(0.8,0.75,0.8),vec3(0.075,0.33,0.67)+0.21);
    } break;

    case uint(3):
    {
      return pal(lum,vec3(0.55),vec3(0.8),vec3(0.29),vec3(0.00,0.05,0.15) + 0.54 );
    } break;

    case uint(4):
    {
      return pal(lum,vec3(0.5),vec3(0.55),vec3(0.45),vec3(0.00,0.10,0.20) + 0.47 );
    } break;

    case uint(5):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(0.9),vec3(0.3,0.20,0.20) + 0.31 );
    } break;

    case uint(6):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(0.9),vec3(0.0,0.10,0.20) + 0.47 );
    } break;

    case uint(7):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(1.0,1.0,0.5),vec3(0.8,0.90,0.30) );
    } break;

    case uint(8):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(1.0,0.7,0.4),vec3(0.0,0.15,0.20) );
    } break;

    case uint(9):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(2.0,1.0,0.0),vec3(0.5,0.20,0.25) );
    } break;

    case uint(10):
    {
      return pal(lum,vec3(0.5),vec3(0.5),vec3(1),vec3(0.0,0.33,0.67));
    } break;

    case uint(11):
    {
      return pal(lum,vec3(0.8,0.5,0.4),vec3(0.2,0.4,0.2),vec3(2.0,1.0,1.0),vec3(0.0,0.25,0.25) );
    } break;
  }

  return vec3(0.0);
}

vec3 Meshify()
{
  color -= color;
  vec2 uv = gl_FragCoord.xy;

  uv /= interlineDistance;
  vec2 p = floor(uv+.5);
  vec2 textureDimension = (textureSize(sourceTexture1, 0) - 1);

  //add .g or nothing 
  #define T(x,y) texture(sourceTexture1, interlineDistance * vec2(x,y) / textureDimension.xy).g 

  #define M(c,T) color += pow(.5+.5*cos( 6.28*(uv-p).c + amplicationFactor * (2.*T-1.) ), meshifyThickness)
  //#define M(c,T) color += .5 + .5 * cos( 2.0 * PI * (uv-p).c + (2.*T-1.) )

  if      ( meshify == uint(0) )
    M( y, T( uv.x, p.y ) ); // modulates  y offset
  else if ( meshify == uint(1) )
    M( x, T( p.x, uv.y ) ); // modulates  x offset
  else
  {
    M( y, T( uv.x, p.y ) );
    M( x, T( p.x, uv.y ) );
  }

  return color;
}

vec3 sketch()
{
    vec2 textureDimension = textureSize(sourceTexture1, 0);
    vec2 pos = gl_FragCoord.xy;
    float weight = 1.0;
    
    for (float j = 0.; j < angleNum; j += 1.)
    {
        vec2 dir = vec2(1, 0);
        pR(dir, j * PI2 / (2. * angleNum));
        
        vec2 grad = vec2(-dir.y, dir.x);
        
        for (float i = -range; i <= range; i += STEP)
        {
            vec2 pos2 = pos + normalize(dir)*i;
            
            if (pos2.y < 0. || pos2.x < 0. || pos2.x > textureDimension.x || pos2.y > textureDimension.y)
                continue;
            
            vec2 g = getGrad(pos2, 1.);
            if (length(g) < MAGIC_GRAD_THRESH)
                continue;
            
            weight -= pow(abs(dot(normalize(grad), normalize(g))), sensitivity) / floor((2. * range + 1.) / STEP) / angleNum;
        }
    }
    
    //vec4 col = texture(sourceTexture1, pos/textureDimension.xy);
    vec4 col = vec4(luminance(texture(sourceTexture1, pos/textureDimension.xy).rgb));
    
    vec4 background = mix(col, vec4(1), MAGIC_COLOR);
 
    // because apparently all shaders need one of these. It's like a law or something.
    float r = length(pos - textureDimension.xy*.5) / textureDimension.x;
    float vign = 1. - r*r*r;
    
    vec4 a = texture(sourceTexture1, pos/textureDimension.xy);
    
    col = vign * mix(vec4(0), background, weight) + a.xxxx/25.;
    return col.rbg;
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

    case 7:
      color = GradientColor();
      break;

    case 8:
      color = sketch();
      break;

//    default:
//      break;
  }
}