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

	vec3 bloom = texture(u_Bloom, v_UV).rgb;
	color += bloom * 0.48;

	color = color / (color + vec3(0.82));
	color = pow(color, vec3(0.94));
	color *= vec3(0.89, 1.02, 1.09);
	float scanline = 0.985 + 0.015 * sin(v_UV.y * u_Resolution.y * 1.55 + u_Time * 2.0);
	float noise = (Random(v_UV * u_Resolution + u_Time) - 0.5) * 0.018;
	color = color * scanline + noise;
	color *= 1.0 - edge * 0.60;

	FragColor = vec4(color, 1.0);
}
