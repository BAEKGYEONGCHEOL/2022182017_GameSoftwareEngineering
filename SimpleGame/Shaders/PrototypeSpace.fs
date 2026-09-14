#version 330

in vec2 v_UV;
layout(location = 0) out vec4 FragColor;

uniform vec2 u_Resolution;
uniform vec2 u_Travel;
uniform float u_Time;

float Hash21(vec2 point)
{
	point = fract(point * vec2(123.34, 345.45));
	point += dot(point, point + 34.345);
	return fract(point.x * point.y);
}

float StarLayer(vec2 uv, float scale, float speed, float threshold)
{
	vec2 movement = vec2(u_Travel.x * 0.0009, u_Travel.y * 0.0014) * speed;
	vec2 cellPosition = uv * scale + movement + vec2(u_Time * 0.015 * speed, -u_Time * 0.035 * speed);
	vec2 cell = floor(cellPosition);
	vec2 local = fract(cellPosition) - 0.5;
	float seed = Hash21(cell);
	vec2 offset = vec2(Hash21(cell + 17.2), Hash21(cell + 41.7)) - 0.5;
	float distanceToStar = length(local - offset * 0.65);
	float star = smoothstep(0.055, 0.0, distanceToStar) * step(threshold, seed);
	float twinkle = 0.72 + 0.28 * sin(u_Time * (2.0 + seed * 5.0) + seed * 30.0);
	return star * twinkle;
}

void main()
{
	vec2 aspectUv = (v_UV - 0.5) * vec2(u_Resolution.x / u_Resolution.y, 1.0);
	float slowCloud = sin(aspectUv.x * 3.4 + u_Time * 0.045) *
		sin(aspectUv.y * 4.7 - u_Time * 0.032);
	float nebula = smoothstep(-0.25, 0.72, slowCloud + sin((aspectUv.x + aspectUv.y) * 2.2) * 0.45);

	vec3 color = vec3(0.003, 0.007, 0.025);
	color += vec3(0.018, 0.030, 0.075) * nebula;
	color += vec3(0.22, 0.42, 0.72) * StarLayer(aspectUv, 18.0, 0.45, 0.82);
	color += vec3(0.48, 0.68, 0.92) * StarLayer(aspectUv, 34.0, 0.85, 0.88);
	color += vec3(0.82, 0.92, 1.00) * StarLayer(aspectUv, 58.0, 1.35, 0.93);

	FragColor = vec4(color, 1.0);
}

