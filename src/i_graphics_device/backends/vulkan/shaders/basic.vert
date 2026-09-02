#version 450

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 fragColour;
layout(location = 1) out float linearDepth;


struct Data {
    mat4 transform;
};

layout(std430, binding = 0, set = 0) readonly buffer ObjectData {
    Data data[];
} objectData;


void main() {
    fragColour = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    Data data = objectData.data[gl_InstanceIndex];
    vec4 vertPos = data.transform * vec4(pos, 1.0f);
    gl_Position = vertPos;
    linearDepth = vertPos.z;
}


