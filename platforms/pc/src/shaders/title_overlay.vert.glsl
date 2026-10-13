#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=0) out vec2 textureUV;
void main() {
    gl_Position = vec4(position.x*2.0-1.0, 1.0-position.y*2.0, 0.0, 1.0);
    textureUV=uv;
}
