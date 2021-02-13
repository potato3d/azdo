#version 450

struct Transform
{
    vec4 m0;
    vec4 m1;
    vec4 m2;
};

// --------------------------------------------------------------------------------------------------------------
// INPUTS
// --------------------------------------------------------------------------------------------------------------

layout(std140) uniform cdata
{
    mat4 view_proj_matrix;
    mat4 normal_matrix;
    mat4 view_matrix;
} input_camera;

layout(std430, binding = 1) buffer transform_in
{
    readonly Transform data[];
} input_transform;

layout(std430, binding = 2) buffer instance_id_in_lod_0
{
    readonly uint data[];
} input_instance_id_lod_0;

layout(std430, binding = 3) buffer instance_id_in_lod_1
{
    readonly uint data[];
} input_instance_id_lod_1;

layout(std430, binding = 4) buffer instance_id_in_lod_2
{
    readonly uint data[];
} input_instance_id_lod_2;

layout(std430, binding = 5) buffer instance_id_in_lod_3
{
    readonly uint data[];
} input_instance_id_lod_3;

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec3 input_normal;
layout(location = 2) in uint input_lod_level;

// --------------------------------------------------------------------------------------------------------------
// OUTPUTS
// --------------------------------------------------------------------------------------------------------------

out fdata
{
    vec3 eye_position;
    vec3 eye_normal;
} vout;

// --------------------------------------------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------------------------------------------

void main()
{
    uint instance_id;

    switch(input_lod_level)
    {
    case 0:
        instance_id = input_instance_id_lod_0.data[gl_InstanceID];
        break;
    case 1:
        instance_id = input_instance_id_lod_1.data[gl_InstanceID];
        break;
    case 2:
        instance_id = input_instance_id_lod_2.data[gl_InstanceID];
        break;
    default:
        instance_id = input_instance_id_lod_3.data[gl_InstanceID];
    }

    const Transform t = input_transform.data[instance_id];
    const mat4 transform = mat4(t.m0,
                                t.m1,
                                t.m2,
                                vec4(0.0f, 0.0f, 0.0f, 1.0f));

    const vec4 world_position_homogeneous = vec4(input_position, 1.0f) * transform;
    const vec4 world_position = vec4(world_position_homogeneous.xyz/world_position_homogeneous.w, 1.0f);
    const vec3 world_normal = mat3(inverse(transform)) * input_normal;

    vout.eye_position = vec3(input_camera.view_matrix * world_position);
    vout.eye_normal = normalize(mat3(input_camera.normal_matrix) * world_normal);

    gl_Position = input_camera.view_proj_matrix * world_position;
}
