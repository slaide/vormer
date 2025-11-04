#version 450

// Hardcoded triangle vertices (in normalized device coordinates)
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),   // bottom center
    vec2(0.5, 0.5),    // top right
    vec2(-0.5, 0.5)    // top left
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
