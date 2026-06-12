#version 450


layout(location = 0) in vec4 inColour;
layout(location = 1) in float linearDepth;

layout(location = 0) out vec4 outColour;


void main() {

    float fogStart = 0.0f;
    float fogEnd = 20.0f;
    float fogFactor = clamp((linearDepth - fogStart) / (fogEnd - fogStart), 0.0f, 1.0f);
    vec3 objectColour = vec3(0.1f, 0.1f, 0.0f);
    vec3 fogColour = vec3(0.7f, 0.8f, 1.0f);
    vec3 finalColour = mix(objectColour, fogColour, fogFactor);

    outColour = vec4(finalColour, 1.0f);
}

