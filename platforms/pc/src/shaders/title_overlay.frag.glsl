#version 450
layout(location=0) in vec2 textureUV;
layout(set=2,binding=0) uniform sampler2D overlayTexture;
layout(location=0) out vec4 fragmentColor;
void main() { fragmentColor=texture(overlayTexture,textureUV); }
