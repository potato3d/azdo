#version 450

layout(std140) uniform cdata
{
    mat4 view_proj_matrix;
    mat4 normal_matrix;
    mat4 view_matrix;
} camera;

layout(std430) buffer tdata
{
    vec4 transforms[];
} tin;

in vec3 in_position;
in vec3 in_normal;

out fdata
{
    vec3 eye_position;
    vec3 eye_normal;
} vout;

void main()
{
    const mat4 transform = mat4(tin.transforms[gl_InstanceID*3+0],
                                tin.transforms[gl_InstanceID*3+1],
                                tin.transforms[gl_InstanceID*3+2],
                                vec4(0.0f, 0.0f, 0.0f, 1.0f));

    const vec4 world_position_homogeneous = vec4(in_position, 1.0f) * transform;
    const vec4 world_position = vec4(world_position_homogeneous.xyz/world_position_homogeneous.w, 1.0f);
    const vec3 world_normal = mat3(inverse(transform)) * in_normal;

    vout.eye_position = vec3(camera.view_matrix * world_position);
    vout.eye_normal = normalize(mat3(camera.normal_matrix) * world_normal);

    gl_Position = camera.view_proj_matrix * world_position;
}
