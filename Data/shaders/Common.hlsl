/*
Copyright(c) 2016-2020 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =================
#include "Common_Struct.hlsl"
#include "Common_Vertex.hlsl"
#include "Common_Buffer.hlsl"
#include "Common_Sampler.hlsl"
#include "Common_Texture.hlsl"
//============================

/*------------------------------------------------------------------------------
    CONSTANTS
------------------------------------------------------------------------------*/
static const float PI       = 3.14159265f;
static const float PI2      = PI * 2;
static const float INV_PI   = 0.31830988f;
static const float FLT_MIN  = 0.00000001f;
static const float FLT_MAX  = 65504.0f;

#define g_texel_size        float2(1.0f / g_resolution.x, 1.0f / g_resolution.y)
#define g_shadow_texel_size (1.0f / g_shadow_resolution)

/*------------------------------------------------------------------------------
    MATH
------------------------------------------------------------------------------*/
inline float min2(float2 value) { return min(value.x, value.y); }
inline float min3(float3 value) { return min(min(value.x, value.y), value.z); }

inline float max2(float2 value) { return max(value.x, value.y); }
inline float max3(float3 value) { return max(max(value.x, value.y), value.z); }

inline bool is_saturated(float value)   { return value == saturate(value); }
inline bool is_saturated(float2 value)  { return is_saturated(value.x) && is_saturated(value.y); }
inline bool is_saturated(float3 value)  { return is_saturated(value.x) && is_saturated(value.y) && is_saturated(value.z); }
inline bool is_saturated(float4 value)  { return is_saturated(value.x) && is_saturated(value.y) && is_saturated(value.z) && is_saturated(value.w); }

/*------------------------------------------------------------------------------
    SATURATE
------------------------------------------------------------------------------*/
inline float    saturate_16(float x)    { return clamp(x, FLT_MIN, FLT_MAX); }
inline float2   saturate_16(float2 x)   { return clamp(x, FLT_MIN, FLT_MAX); }
inline float3   saturate_16(float3 x)   { return clamp(x, FLT_MIN, FLT_MAX); }
inline float4   saturate_16(float4 x)   { return clamp(x, FLT_MIN, FLT_MAX); }

/*------------------------------------------------------------------------------
    GAMMA CORRECTION
------------------------------------------------------------------------------*/
inline float4 degamma(float4 color) { return pow(abs(color), g_gamma); }
inline float3 degamma(float3 color) { return pow(abs(color), g_gamma); }
inline float4 gamma(float4 color)   { return pow(abs(color), 1.0f / g_gamma); }
inline float3 gamma(float3 color)   { return pow(abs(color), 1.0f / g_gamma); }

/*------------------------------------------------------------------------------
    PACKING
------------------------------------------------------------------------------*/
inline float3 unpack(float3 value)  { return value * 2.0f - 1.0f; }
inline float3 pack(float3 value)    { return value * 0.5f + 0.5f; }
inline float2 unpack(float2 value)  { return value * 2.0f - 1.0f; }
inline float2 pack(float2 value)    { return value * 0.5f + 0.5f; }

/*------------------------------------------------------------------------------
    PROJECT
------------------------------------------------------------------------------*/
inline float3 project(float3 position, matrix transform)
{
    float4 projectedCoords  = mul(float4(position, 1.0f), transform);
    projectedCoords.xyz     /= projectedCoords.w;
    projectedCoords.xy      = projectedCoords.xy * float2(0.5f, -0.5f) + 0.5f;

    return projectedCoords.xyz;
}

inline float2 project_uv(float3 position, matrix transform)
{
    return project(position, transform).xy;
}

inline float project_depth(float3 position, matrix transform)
{
    return project(position, transform).z;
}

/*------------------------------------------------------------------------------
    NORMAL
------------------------------------------------------------------------------*/
// No decoding required (just normalise)
inline float3 normal_decode(float3 normal)  { return normalize(normal); }
// No encoding required (just normalise)
inline float3 normal_encode(float3 normal)  { return normalize(normal); }

inline float3 get_normal(float2 uv)
{
    //float2 pos = clamp(uv * g_resolution, 0.0f, g_resolution);
    //return normal_decode(tex_normal.Load(int3(pos, 0)).rgb);
    return tex_normal.SampleLevel(sampler_bilinear_clamp, uv, 0).xyz;
}

inline float3 get_normal_view_space(float2 uv)
{
    float3 normal_world_space = get_normal(uv);
    return normalize(mul(float4(normal_world_space, 0.0f), g_view).xyz);
}

inline float3x3 makeTBN(float3 n, float3 t)
{
    // re-orthogonalize T with respect to N
    t = normalize(t - dot(t, n) * n);
    // compute bitangent
    float3 b = cross(n, t);
    // create matrix
    return float3x3(t, b, n); 
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
inline float3 tangent_to_world(float3 position_world, float3 tangent, float3 normal, float2 uv)
{
    float3 dp1  = ddx(position_world);
    float3 dp2  = ddy(position_world);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 N        = normalize(normal);
    float3 T        = normalize(dp1 * duv2.y - dp2 * duv1.y);
    float3 B        = normalize(cross(N, T));
    float3x3 TBN    = float3x3(T, B, N);

    return normalize(mul(tangent, TBN).xyz);
}

/*------------------------------------------------------------------------------
    DEPTH
------------------------------------------------------------------------------*/
inline float get_depth(float2 uv)
{
    //float2 pos = clamp(uv * g_resolution, 0.0f, g_resolution);
    //return tex_depth.Load(int3(pos, 0)).r;
    return tex_depth.SampleLevel(sampler_bilinear_clamp, uv, 0).r;
}

inline float get_linear_depth(float z, float near, float far)
{
    float z_b = z;
    float z_n = 2.0f * z_b - 1.0f;
    return 2.0f * far * near / (near + far - z_n * (near - far));
}

inline float get_linear_depth(float z)
{
    return get_linear_depth(z, g_camera_near, g_camera_far);
}

inline float get_linear_depth(float2 uv)
{
    float depth = get_depth( uv);
    return get_linear_depth(depth);
}

/*------------------------------------------------------------------------------
    POSITION
------------------------------------------------------------------------------*/
inline float3 get_position(float z, float2 uv)
{
    // Reconstruct position from depth

    float x             = uv.x * 2.0f - 1.0f;
    float y             = (1.0f - uv.y) * 2.0f - 1.0f;
    float4 pos_clip     = float4(x, y, z, 1.0f);
    float4 pos_world    = mul(pos_clip, g_viewProjectionInv);   
    return pos_world.xyz / pos_world.w;  
}

inline float3 get_position(float2 uv)
{
    float depth = get_depth(uv);
    return get_position(depth, uv);
}

inline float3 get_position_view_space(float2 uv)
{
    float3 position_world_space = get_position(uv);
    return mul(float4(position_world_space, 1.0f), g_view).xyz;
}

/*------------------------------------------------------------------------------
    VIEW DIRECTION
------------------------------------------------------------------------------*/
inline float3 get_view_direction( float depth, float2 uv)
{
    float3 position_world = get_position(depth, uv);
    return normalize(position_world - g_camera_position.xyz);
}

inline float3 get_view_direction(float2 uv)
{
    float depth = get_depth(uv);
    return get_view_direction(depth, uv);
}

/*------------------------------------------------------------------------------
    DIRECTION TO UV
------------------------------------------------------------------------------*/
inline float2 direction_sphere_uv(float3 direction)
{
    float n = length(direction.xz);
    float2 uv = float2((n > 0.0000001) ? direction.x / n : 0.0, direction.y);
    uv = acos(uv) * INV_PI;
    uv.x = (direction.z > 0.0) ? uv.x * 0.5 : 1.0 - (uv.x * 0.5);
    uv.x = 1.0 - uv.x;
    
    return uv;
}

inline uint direction_to_cube_face_index(const float3 direction)
{
    float3 direction_abs = abs(direction);
    float max_coordinate = max3(direction_abs);
    
    if (max_coordinate == direction_abs.x)
    {
        return direction_abs.x == direction.x ? 0 : 1;
    }
    else if (max_coordinate == direction_abs.y)
    {
        return direction_abs.y == direction.y ? 2 : 3;
    }
    else
    {
        return direction_abs.z == direction.z ? 4 : 5;
    }
    
    return 0;
}

/*------------------------------------------------------------------------------
    LUMINANCE
------------------------------------------------------------------------------*/
static const float3 lumCoeff = float3(0.299f, 0.587f, 0.114f);

inline float luminance(float3 color)
{
    return max(dot(color, lumCoeff), 0.0001f);
}

inline float luminance(float4 color)
{
    return max(dot(color.rgb, lumCoeff), 0.0001f);
}

/*------------------------------------------------------------------------------
    RANDOM/SAMPLING
------------------------------------------------------------------------------*/
inline float random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

// Based on interleaved gradient function from Jimenez 2014 http://goo.gl/eomGso
inline float interleaved_gradient_noise(float2 position_screen)
{
    position_screen += g_frame * any(g_taa_jitter_offset); // temporal factor
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(position_screen, magic.xy)));
}

inline float blue_noise(float2 uv)
{
    static const float c_goldenRatioConjugate = 0.61803398875f; // also just fract(goldenRatio)
    static const float2 noise_scale = float2(g_resolution.x / 256.0f, g_resolution.y / 256.0f);
    float noise = tex_normal_noise.Sample(sampler_bilinear_wrap, uv * noise_scale).r;
    return frac(noise + float(g_frame) * c_goldenRatioConjugate * any(g_taa_jitter_offset));
}



/*------------------------------------------------------------------------------
    OCCLUSION/SHADOWING
------------------------------------------------------------------------------*/

// From Activision GTAO paper: https://www.activision.com/cdn/research/s2016_pbs_activision_occlusion.pptx
inline float3 MultiBounceAO(float visibility, float3 albedo)
{
    float3 a = 2.0404 * albedo - 0.3324;
    float3 b = -4.7951 * albedo + 0.6417;
    float3 c = 2.7552 * albedo + 0.6903;
	
    float x = visibility;
    return max(x, ((x * a + b) * x + c) * x);
}

// The Technical Art of Uncharted 4 - http://advances.realtimerendering.com/other/2016/naughty_dog/index.html
float microw_shadowing_nt(float n_dot_l, float ao)
{
    float aperture = 2.0f * ao * ao;
    return saturate(abs(n_dot_l) + aperture - 1.0f);
}

// Chan 2018, "Material Advances in Call of Duty: WWII"
float microw_shadowing_cod(float n_dot_l, float visibility)
{
    float aperture = rsqrt(1.0 - visibility);
    float microShadow = saturate(n_dot_l * aperture);
    return microShadow * microShadow;
}

/*------------------------------------------------------------------------------
    MISC
------------------------------------------------------------------------------*/

inline float3 energy_conservation(float3 F, float metallic = 0.0f)
{
    float3 kS = F;          // The energy of light that gets reflected - Equal to Fresnel
    float3 kD = 1.0f - kS;  // Remaining energy, light that gets refracted          
    kD *= 1.0f - metallic;  // Multiply kD by the inverse metalness such that only non-metals have diffuse lighting
    
    return kD;
}

inline float screen_fade(float2 uv)
{
    float2 fade = max(12.0f * abs(uv - 0.5f) - 5.0f, 0.0f);
    return saturate(1.0 - dot(fade, fade));
}
