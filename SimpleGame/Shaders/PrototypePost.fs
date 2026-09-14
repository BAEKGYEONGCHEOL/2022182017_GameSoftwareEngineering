#version 330

in vec2 v_UV;
layout(location = 0) out vec4 FragColor;

uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform vec2 u_Resolution;
uniform float u_Time;

float Random(vec2 p)
{
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 AcesToneMap(vec3 value)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((value * (a * value + b)) / (value * (c * value + d) + e), 0.0, 1.0);
}

void main()
{
	vec2 pixel = 1.0 / u_Resolution;
	vec2 centered = v_UV - 0.5;
	float edge = smoothstep(0.15, 0.72, length(centered * vec2(1.0, 0.78)));
	float aberration = 1.2 + edge * 1.8;

	vec3 color;
	color.r = texture(u_Scene, v_UV + vec2(pixel.x * aberration, 0.0)).r;
	color.g = texture(u_Scene, v_UV).g;
	color.b = texture(u_Scene, v_UV - vec2(pixel.x * aberration, 0.0)).b;
	vec3 localAverage =
		texture(u_Scene, v_UV + vec2(pixel.x, 0.0)).rgb +
		texture(u_Scene, v_UV - vec2(pixel.x, 0.0)).rgb +
		texture(u_Scene, v_UV + vec2(0.0, pixel.y)).rgb +
		texture(u_Scene, v_UV - vec2(0.0, pixel.y)).rgb;
	localAverage *= 0.25;
	color += (color - localAverage) * 0.22;

	vec3 bloom = texture(u_Bloom, v_UV).rgb;
	color += bloom * 0.64;

	color = AcesToneMap(max(color, vec3(0.0)) * 1.14);
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color = mix(vec3(luminance), color, 1.08);
	color = pow(color, vec3(0.96));
	color *= vec3(0.90, 1.01, 1.07);
	float scanline = 0.988 + 0.012 * sin(v_UV.y * u_Resolution.y * 1.55 + u_Time * 2.0);
	float noise = (Random(v_UV * u_Resolution + u_Time) - 0.5) * 0.012;
	color = color * scanline + noise;
	color *= 1.0 - edge * 0.46;

	FragColor = vec4(color, 1.0);
}
