#version 330

in vec2 a_Position;
uniform vec2 u_ScreenSize;

void main()
{
	vec2 ndc = vec2(a_Position.x * 2.0 / u_ScreenSize.x, a_Position.y * 2.0 / u_ScreenSize.y);
	gl_Position = vec4(ndc, 0.0, 1.0);
}
