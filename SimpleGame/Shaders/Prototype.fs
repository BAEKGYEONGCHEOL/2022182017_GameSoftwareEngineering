#version 330

layout(location = 0) out vec4 FragColor;
in vec4 v_Color;
in vec2 v_EffectCoordinate;
flat in float v_EffectType;

void main()
{
	float alpha = v_Color.a;
	if (v_EffectType > 0.5)
	{
		float distanceFromCenter = length(v_EffectCoordinate);
		alpha *= 1.0 - smoothstep(0.28, 1.0, distanceFromCenter);
	}
	FragColor = vec4(v_Color.rgb, alpha);
}
