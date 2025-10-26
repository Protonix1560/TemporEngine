#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColour;

layout(location = 0) out vec4 outColour;


layout(push_constant) uniform PushConst {
    mat4 mvp;
} pc;


void main() {
    outColour = inColour;
    vec4 vertPos = pc.mvp * vec4(inPos, 1.0f);
    vertPos.x = -vertPos.x;
    gl_Position = vertPos;
}


