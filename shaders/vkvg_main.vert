/*
 * Copyright (c) 2018-2019 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in		vec2 inPos;
layout (location = 1) in lowp   vec4 inColor;
layout (location = 2) in		vec2 inUV;
layout (location = 3) in		int	 inUVZ;

layout (location = 0) out		vec3 outUV;
layout (location = 1) out		vec4 outSrc;
layout (location = 2) out flat  int outPatType;
layout (location = 3) out		mat3x2 outMat;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(push_constant) uniform PushConsts {
	vec4 source;
	vec2 size;
	int  srcType;
	int  fullScreenQuad;
	mat3x2 mat;
	mat3x2 matInv;
} pc;

#define SOLID			0
#define SURFACE			1
#define LINEAR			2
#define RADIAL			3
#define MESH			4
#define RASTER_SOURCE	5

void main()
{
	outPatType	= pc.srcType;
	outSrc		= inColor;
	outMat		= pc.matInv;

	if (pc.fullScreenQuad != 0) {
		gl_Position = vec4(vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0f + -1.0f, 0.0f, 1.0f);
		outUV = vec3((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2, -2);
		//outUV = vec3(0,0,-1);
		return;
	}

	vec2 p0 = inPos;

	if (pc.srcType == SURFACE) {
		vec2 uv = vec2(
			pc.matInv[0][0] * p0.x + pc.matInv[1][0] * p0.y + pc.matInv[2][0],
			pc.matInv[0][1] * p0.x + pc.matInv[1][1] * p0.y + pc.matInv[2][1]
		);
		uv -= pc.source.xy;
		outUV = vec3(uv.x / pc.source.z, uv.y / pc.source.w ,-1);
	}else
		outUV = vec3(inUV, inUVZ);

	gl_Position = vec4(p0 * vec2(2) / pc.size - vec2(1), 0.0, 1.0);
}
