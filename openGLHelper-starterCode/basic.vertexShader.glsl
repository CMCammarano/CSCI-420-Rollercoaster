#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uvs;
out vec2 frag_uv;
out vec4 frag_colors;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main() {
	// compute the transformed and projected vertex position (into gl_Position) 
	// compute the vertex color (into col)
	// students need to implement this

	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);
	frag_uv = uvs;
}

