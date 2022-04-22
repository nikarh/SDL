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

/**
 *  \file SDL_opengles.h
 *
 *  This is a simple file to encapsulate the OpenGL ES 1.X API headers.
 */
#include "SDL_config.h"

#ifdef __IPHONEOS__
#include <OpenGLES/ES1/gl.h>
#include <OpenGLES/ES1/glext.h>
#elif defined(SDL_VIDEO_VITA_VGL)
#include <vitaGL.h>

#define GL_FUNC_ADD_OES                                         0x8006
#define GL_FUNC_SUBTRACT_OES                                    0x800A
#define GL_FUNC_REVERSE_SUBTRACT_OES                            0x800B
#define GL_UNPACK_ALIGNMENT                                     0x0CF5
#define GL_FRAMEBUFFER_OES                                      0x8D40
#define GL_COLOR_ATTACHMENT0_OES                                0x8CE0
#define GL_FRAMEBUFFER_COMPLETE_OES                             0x8CD5
#define GL_FRAMEBUFFER_BINDING_OES                              0x8CA6
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
