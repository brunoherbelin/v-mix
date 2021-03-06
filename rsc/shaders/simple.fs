#version 330 core

out vec4 FragColor;

in vec4 vertexColor;

uniform vec3 iResolution;      // viewport resolution (in pixels)

uniform vec4 color;            // drawing color

void main()
{
    vec4 c = color * vertexColor;
    FragColor = vec4( c.r * c.a, c.g * c.a, c.b * c.a, c.a);
}

// RGBA 2 YUVA converter
//vec4 yuva = vec4(0.0);
//yuva.x = rgba.r * 0.299 + rgba.g * 0.587 + rgba.b * 0.114;
//yuva.y = rgba.r * -0.169 + rgba.g * -0.331 + rgba.b * 0.5 + 0.5;
//yuva.z = rgba.r * 0.5 + rgba.g * -0.419 + rgba.b * -0.081 + 0.5;
//yuva.w = 1.0;
