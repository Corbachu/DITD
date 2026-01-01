//----------------------------------------------------------------------------
//  Dream In The Dark embedded Shaders (Rendering)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025  yaz0r/jimmu/FITD Team
//  Copyright (C) 1999-2025  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
#include "embeddedShadersMacro.h"
#include "config.h"

#if BGFX_PLATFORM_SUPPORTS_SPIRV
#include "shaders/generated/spirv/ui_vs.sc.bin.h"
#include "shaders/generated/spirv/ui_ps.sc.bin.h"
#include "shaders/generated/spirv/background_vs.sc.bin.h"
#include "shaders/generated/spirv/background_ps.sc.bin.h"
#include "shaders/generated/spirv/maskBackground_vs.sc.bin.h"
#include "shaders/generated/spirv/maskBackground_ps.sc.bin.h"
#include "shaders/generated/spirv/ramp_vs.sc.bin.h"
#include "shaders/generated/spirv/ramp_ps.sc.bin.h"
#include "shaders/generated/spirv/noise_vs.sc.bin.h"
#include "shaders/generated/spirv/noise_ps.sc.bin.h"
#include "shaders/generated/spirv/flat_vs.sc.bin.h"
#include "shaders/generated/spirv/flat_ps.sc.bin.h"
#include "shaders/generated/spirv/sphere_vs.sc.bin.h"
#include "shaders/generated/spirv/sphere_ps.sc.bin.h"
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#include "shaders/generated/metal/ui_vs.sc.bin.h"
#include "shaders/generated/metal/ui_ps.sc.bin.h"
#include "shaders/generated/metal/background_vs.sc.bin.h"
#include "shaders/generated/metal/background_ps.sc.bin.h"
#include "shaders/generated/metal/maskBackground_vs.sc.bin.h"
#include "shaders/generated/metal/maskBackground_ps.sc.bin.h"
#include "shaders/generated/metal/ramp_vs.sc.bin.h"
#include "shaders/generated/metal/ramp_ps.sc.bin.h"
#include "shaders/generated/metal/noise_vs.sc.bin.h"
#include "shaders/generated/metal/noise_ps.sc.bin.h"
#include "shaders/generated/metal/flat_vs.sc.bin.h"
#include "shaders/generated/metal/flat_ps.sc.bin.h"
#include "shaders/generated/metal/sphere_vs.sc.bin.h"
#include "shaders/generated/metal/sphere_ps.sc.bin.h"
#endif

#if BGFX_PLATFORM_SUPPORTS_GLSL
#include "shaders/generated/glsl/ui_vs.sc.bin.h"
#include "shaders/generated/glsl/ui_ps.sc.bin.h"
#include "shaders/generated/glsl/background_vs.sc.bin.h"
#include "shaders/generated/glsl/background_ps.sc.bin.h"
#include "shaders/generated/glsl/maskBackground_vs.sc.bin.h"
#include "shaders/generated/glsl/maskBackground_ps.sc.bin.h"
#include "shaders/generated/glsl/ramp_vs.sc.bin.h"
#include "shaders/generated/glsl/ramp_ps.sc.bin.h"
#include "shaders/generated/glsl/noise_vs.sc.bin.h"
#include "shaders/generated/glsl/noise_ps.sc.bin.h"
#include "shaders/generated/glsl/flat_vs.sc.bin.h"
#include "shaders/generated/glsl/flat_ps.sc.bin.h"
#include "shaders/generated/glsl/sphere_vs.sc.bin.h"
#include "shaders/generated/glsl/sphere_ps.sc.bin.h"
#endif

#if BGFX_PLATFORM_SUPPORTS_DXBC
#include "shaders/generated/dx11/ui_vs.sc.bin.h"
#include "shaders/generated/dx11/ui_ps.sc.bin.h"
#include "shaders/generated/dx11/background_vs.sc.bin.h"
#include "shaders/generated/dx11/background_ps.sc.bin.h"
#include "shaders/generated/dx11/maskBackground_vs.sc.bin.h"
#include "shaders/generated/dx11/maskBackground_ps.sc.bin.h"
#include "shaders/generated/dx11/ramp_vs.sc.bin.h"
#include "shaders/generated/dx11/ramp_ps.sc.bin.h"
#include "shaders/generated/dx11/noise_vs.sc.bin.h"
#include "shaders/generated/dx11/noise_ps.sc.bin.h"
#include "shaders/generated/dx11/flat_vs.sc.bin.h"
#include "shaders/generated/dx11/flat_ps.sc.bin.h"
#include "shaders/generated/dx11/sphere_vs.sc.bin.h"
#include "shaders/generated/dx11/sphere_ps.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(ui_vs),
    BGFX_EMBEDDED_SHADER(ui_ps),
    BGFX_EMBEDDED_SHADER(background_vs),
	BGFX_EMBEDDED_SHADER(background_ps),
	BGFX_EMBEDDED_SHADER(maskBackground_vs),
	BGFX_EMBEDDED_SHADER(maskBackground_ps),
	BGFX_EMBEDDED_SHADER(flat_vs),
	BGFX_EMBEDDED_SHADER(flat_ps),
	BGFX_EMBEDDED_SHADER(noise_vs),
	BGFX_EMBEDDED_SHADER(noise_ps),
	BGFX_EMBEDDED_SHADER(ramp_vs),
	BGFX_EMBEDDED_SHADER(ramp_ps),
    BGFX_EMBEDDED_SHADER(sphere_vs),
    BGFX_EMBEDDED_SHADER(sphere_ps),

	BGFX_EMBEDDED_SHADER_END()
};

bgfx::ProgramHandle loadBgfxProgram(const std::string& VSFile, const std::string& PSFile)
{
	bgfx::RendererType::Enum type = bgfx::getRendererType();

	bgfx::ProgramHandle ProgramHandle = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, VSFile.c_str())
		, bgfx::createEmbeddedShader(s_embeddedShaders, type, PSFile.c_str())
		, true
	);
	assert(bgfx::isValid(ProgramHandle));
	return ProgramHandle;
}
