#version 330

in vec2 v_UV;
layout(location = 0) out vec4 FragColor;

uniform sampler2D u_Image;
uniform vec2 u_Resolution;
uniform vec2 u_Direction;
uniform int u_ExtractBright;

void main()
{
	vec2 stepUV = u_Direction / u_Resolution;
	vec3 color = texture(u_Image, v_UV).rgb * 0.227027;
	color += texture(u_Image, v_UV + stepUV * 1.384615).rgb * 0.316216;
	color += texture(u_Image, v_UV - stepUV * 1.384615).rgb * 0.316216;
	color += texture(u_Image, v_UV + stepUV * 3.230769).rgb * 0.070270;
	color += texture(u_Image, v_UV - stepUV * 3.230769).rgb * 0.070270;
	if (u_ExtractBright == 1)
	{
		float brightness = max(color.r, max(color.g, color.b));
		color *= smoothstep(0.28, 0.72, brightness);
	}
	FragColor = vec4(color, 1.0);
}
