#version 330 core

in vec4 frag_color;
out vec3 color;

void main() {
	color = vec3(frag_color);
}

