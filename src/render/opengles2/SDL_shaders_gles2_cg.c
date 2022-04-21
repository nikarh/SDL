/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED && (SDL_VIDEO_VITA_VGL || SDL_VIDEO_VITA_PIB)

#include "SDL_video.h"
#include "SDL_opengles2.h"
#include "SDL_shaders_gles2.h"
#include "SDL_stdinc.h"

/*************************************************************************************************
 * Vertex/fragment shader source                                                                 *
 *************************************************************************************************/
static const Uint8 GLES2_Vertex_Default[] = " \
    struct _Output { \
      float2 v_texCoord : TEXCOORD0; \
      float4 v_color : COLOR; \
      float4 position : POSITION; \
      float pointsize    : PSIZE; \
    }; \
\
    _Output main( \
        uniform float4x4 u_projection, \
        float2 a_position, \
        float4 a_color, \
        float2 a_texCoord \
    ) \
    { \
        _Output OUT; \
\
        OUT.v_texCoord = a_texCoord; \
        OUT.v_color = a_color; \
        OUT.position =  mul(float4(a_position, 0.0, 1.0), u_projection); \
        OUT.pointsize = 1.0; \
        return OUT; \
    } \
";

static const Uint8 GLES2_Fragment_Solid[] = " \
    float4 main(float4 v_color : COLOR) : COLOR \
    { \
        return v_color; \
    } \
";

static const Uint8 GLES2_Fragment_TextureABGR[] = " \
    float4 main(uniform sampler2D u_texture, float4 v_color : COLOR, float2 v_texCoord : TEXCOORD0 ) : COLOR \
    { \
        float4 color = tex2D(u_texture, v_texCoord); \
        return color * v_color; \
    } \
";

/* ARGB to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureARGB[] = " \
    float4 main(uniform sampler2D u_texture, float4 v_color : COLOR, float2 v_texCoord : TEXCOORD0 ) : COLOR \
    { \
        float4 abgr = tex2D(u_texture, v_texCoord); \
        float4 color = abgr; \
        color.r = abgr.b; \
        color.b = abgr.r; \
        return color * v_color; \
    } \
";

/* RGB to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureRGB[] = " \
    float4 main(uniform sampler2D u_texture, float4 v_color : COLOR, float2 v_texCoord : TEXCOORD0 ) : COLOR \
    { \
        float4 abgr = tex2D(u_texture, v_texCoord); \
        float4 color = abgr; \
        color.r = abgr.b; \
        color.b = abgr.r; \
        color.a = 1.0; \
        return color * v_color; \
    } \
";

/* BGR to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureBGR[] = " \
    float4 main(uniform sampler2D u_texture, float4 v_color : COLOR, float2 v_texCoord : TEXCOORD0 ) : COLOR \
    { \
        float4 abgr = tex2D(u_texture, v_texCoord); \
        float4 color = abgr; \
        color.a = 1.0; \
        return color * v_color; \
    } \
";

#if SDL_HAVE_YUV

#define JPEG_SHADER_CONSTANTS                                        \
"    // YUV offset \n"                                               \
"    const float3 offset = float3(0, -0.501960814, -0.501960814);\n" \
"\n"                                                                 \
"    // RGB coefficients \n"                                         \
"    const float3x3 matrix = float3x3( 1,       1,        1,\n"      \
"                                  0,      -0.3441,   1.772,\n"      \
"                                  1.402,  -0.7141,   0);\n"         \

#define BT601_SHADER_CONSTANTS                                       \
"    // YUV offset \n"                                               \
"    const float3 offset = vec3(-0.0627451017, -0.501960814, -0.501960814);\n" \
"\n"                                                                 \
"    // RGB coefficients \n"                                         \
"    const float3x3 matrix = float3x3( 1.1644,  1.1644,   1.1644,\n" \
"                                      0,      -0.3918,   2.0172,\n" \
"                                      1.596,  -0.813,    0);\n"     \

#define BT709_SHADER_CONSTANTS                                       \
"    // YUV offset \n"                                               \
"    const float3 offset = float3(-0.0627451017, -0.501960814, -0.501960814);\n" \
"\n"                                                                 \
"    // RGB coefficients \n"                                         \
"    const float3x3 matrix = float3x3( 1.1644,  1.1644,   1.1644,\n" \
"                                      0,      -0.2132,   2.1124,\n" \
"                                      1.7927, -0.5329,   0);\n"     \


#define YUV_SHADER_PROLOGUE                                     \
"float4 main(\n"                                                \
"    float2 v_texCoord : TEXCOORD0,\n"                          \
"    uniform sampler2D u_texture,\n"                            \
"    uniform sampler2D u_texture_u,\n"                          \
"    uniform sampler2D u_texture_v,\n"                          \
"    uniform vec4 u_modulation,\n"                              \
") : COLOR {\n"                                                 \

#define YUV_SHADER_BODY                                         \
"    float3 yuv;\n"                                             \
"    half3 rgb;\n"                                              \
"\n"                                                            \
"    // Get the YUV values \n"                                  \
"    yuv.x = tex2D(u_texture,   v_texCoord).r;\n"               \
"    yuv.y = tex2D(u_texture_u, v_texCoord).r;\n"               \
"    yuv.z = tex2D(u_texture_v, v_texCoord).r;\n"               \
"\n"                                                            \
"    // Do the color transform \n"                              \
"    yuv += offset;\n"                                          \
"    rgb = mul(yuv, matrix);\n"                                 \
"\n"                                                            \
"    // That was easy. :) \n"                                   \
"    return float4(rgb, 1.0) * u_modulation;\n"                 \
"}"                                                             \

#define NV12_RA_SHADER_BODY                                     \
"    float3 yuv;\n"                                             \
"    half3 rgb;\n"                                              \
"\n"                                                            \
"    // Get the YUV values \n"                                  \
"    yuv.x = tex2D(u_texture,   v_texCoord).r;\n"               \
"    yuv.yz = tex2D(u_texture_u, v_texCoord).ra;\n"             \
"\n"                                                            \
"    // Do the color transform \n"                              \
"    yuv += offset;\n"                                          \
"    rgb = mul(yuv, matrix);\n"                                 \
"\n"                                                            \
"    // That was easy. :) \n"                                   \
"    return float4(rgb, 1.0) * u_modulation;\n"                 \
"}"                                                                 \

#define NV12_RG_SHADER_BODY                                     \
"    float3 yuv;\n"                                             \
"    half3 rgb;\n"                                              \
"\n"                                                            \
"    // Get the YUV values \n"                                  \
"    yuv.x = tex2D(u_texture,   v_texCoord).r;\n"               \
"    yuv.yz = tex2D(u_texture_u, v_texCoord).ra;\n"             \
"\n"                                                            \
"    // Do the color transform \n"                              \
"    yuv += offset;\n"                                          \
"    rgb = mul(yuv, matrix);\n"                                 \
"\n"                                                            \
"    // That was easy. :) \n"                                   \
"    return float4(rgb, 1.0) * u_modulation;\n"                 \
"}"                                                             \

#define NV21_SHADER_BODY                                        \
"    float3 yuv;\n"                                             \
"    half3 rgb;\n"                                              \
"\n"                                                            \
"    // Get the YUV values \n"                                  \
"    yuv.x = tex2D(u_texture,   v_texCoord).r;\n"               \
"    yuv.yz = tex2D(u_texture_u, v_texCoord).ar;\n"             \
"\n"                                                            \
"    // Do the color transform \n"                              \
"    yuv += offset;\n"                                          \
"    rgb = mul(yuv, matrix);\n"                                 \
"\n"                                                            \
"    // That was easy. :) \n"                                   \
"    return float4(rgb, 1.0) * u_modulation;\n"                 \
"}"                                                             \

/* YUV to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureYUVJPEG[] = \
        YUV_SHADER_PROLOGUE \
        JPEG_SHADER_CONSTANTS \
        YUV_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureYUVBT601[] = \
        YUV_SHADER_PROLOGUE \
        BT601_SHADER_CONSTANTS \
        YUV_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureYUVBT709[] = \
        YUV_SHADER_PROLOGUE \
        BT709_SHADER_CONSTANTS \
        YUV_SHADER_BODY \
;

/* NV12 to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureNV12JPEG[] = \
        YUV_SHADER_PROLOGUE \
        JPEG_SHADER_CONSTANTS \
        NV12_RA_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV12BT601_RA[] = \
        YUV_SHADER_PROLOGUE \
        BT601_SHADER_CONSTANTS \
        NV12_RA_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV12BT601_RG[] = \
        YUV_SHADER_PROLOGUE \
        BT601_SHADER_CONSTANTS \
        NV12_RG_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV12BT709_RA[] = \
        YUV_SHADER_PROLOGUE \
        BT709_SHADER_CONSTANTS \
        NV12_RA_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV12BT709_RG[] = \
        YUV_SHADER_PROLOGUE \
        BT709_SHADER_CONSTANTS \
        NV12_RG_SHADER_BODY \
;

/* NV21 to ABGR conversion */
static const Uint8 GLES2_Fragment_TextureNV21JPEG[] = \
        YUV_SHADER_PROLOGUE \
        JPEG_SHADER_CONSTANTS \
        NV21_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV21BT601[] = \
        YUV_SHADER_PROLOGUE \
        BT601_SHADER_CONSTANTS \
        NV21_SHADER_BODY \
;
static const Uint8 GLES2_Fragment_TextureNV21BT709[] = \
        YUV_SHADER_PROLOGUE \
        BT709_SHADER_CONSTANTS \
        NV21_SHADER_BODY \
;
#endif

/* Custom Android video format texture */
static const Uint8 GLES2_Fragment_TextureExternalOES[] = " \
    float4 main( \
        float2 v_texCoord : TEXCOORD0, \
        uniform sampler2D u_texture, \
        uniform float4 u_modulation \
    ) { \
        return tex2D(u_texture, v_texCoord) * u_modulation; \
    } \
";


/*************************************************************************************************
 * Shader selector                                                                               *
 *************************************************************************************************/

const Uint8 *GLES2_GetShader(GLES2_ShaderType type)
{
    switch (type) {
    case GLES2_SHADER_VERTEX_DEFAULT:
        return GLES2_Vertex_Default;
    case GLES2_SHADER_FRAGMENT_SOLID:
        return GLES2_Fragment_Solid;
    case GLES2_SHADER_FRAGMENT_TEXTURE_ABGR:
        return GLES2_Fragment_TextureABGR;
    case GLES2_SHADER_FRAGMENT_TEXTURE_ARGB:
        return GLES2_Fragment_TextureARGB;
    case GLES2_SHADER_FRAGMENT_TEXTURE_RGB:
        return GLES2_Fragment_TextureRGB;
    case GLES2_SHADER_FRAGMENT_TEXTURE_BGR:
        return GLES2_Fragment_TextureBGR;
#if SDL_HAVE_YUV
    case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG:
        return GLES2_Fragment_TextureYUVJPEG;
    case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT601:
        return GLES2_Fragment_TextureYUVBT601;
    case GLES2_SHADER_FRAGMENT_TEXTURE_YUV_BT709:
        return GLES2_Fragment_TextureYUVBT709;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_JPEG:
        return GLES2_Fragment_TextureNV12JPEG;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RA_BT601:
        return GLES2_Fragment_TextureNV12BT601_RA;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RG_BT601:
        return GLES2_Fragment_TextureNV12BT601_RG;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RA_BT709:
        return GLES2_Fragment_TextureNV12BT709_RA;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV12_RG_BT709:
        return GLES2_Fragment_TextureNV12BT709_RG;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_JPEG:
        return GLES2_Fragment_TextureNV21JPEG;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT601:
        return GLES2_Fragment_TextureNV21BT601;
    case GLES2_SHADER_FRAGMENT_TEXTURE_NV21_BT709:
        return GLES2_Fragment_TextureNV21BT709;
#endif
    case GLES2_SHADER_FRAGMENT_TEXTURE_EXTERNAL_OES:
        return GLES2_Fragment_TextureExternalOES;
    default:
        return NULL;
    }
}

#endif /* SDL_VIDEO_RENDER_OGL_ES2 && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
