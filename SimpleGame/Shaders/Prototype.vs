#version 330

in vec2 a_Position;
in vec4 a_Color;
in vec2 a_EffectCoordinate;
in float a_EffectType;
uniform vec2 u_ScreenSize;
out vec4 v_Color;
out vec2 v_EffectCoordinate;
flat out float v_EffectType;

void main()
{
	vec2 ndc = vec2(a_Position.x * 2.0 / u_ScreenSize.x, a_Position.y * 2.0 / u_ScreenSize.y);
	gl_Position = vec4(ndc, 0.0, 1.0);
	v_Color = a_Color;
	v_EffectCoordinate = a_EffectCoordinate;
	v_EffectType = a_EffectType;
}
