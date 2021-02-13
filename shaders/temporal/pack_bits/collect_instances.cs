#version 450

struct Bound
{
    vec4 bmin;
    vec4 bmax;
};

// --------------------------------------------------------------------------------------------------------------
// INPUTS
// --------------------------------------------------------------------------------------------------------------

layout(local_size_x = 256) in;

layout(location = 0) uniform uint instance_count;

layout(std140) uniform camera_data
{
    mat4 view_proj_matrix;
    mat4 normal_matrix;
    mat4 view_matrix;
} camera;

layout(std430, binding = 0) buffer bound_in
{
    readonly Bound data[];
} input_bound;

layout(std430, binding = 11) buffer bits_curr_in
{
    readonly uint data[];
} input_bits_curr;

// --------------------------------------------------------------------------------------------------------------
// OUTPUTS
// --------------------------------------------------------------------------------------------------------------

layout(binding = 0) uniform atomic_uint instance_count_lod_0;
layout(binding = 1) uniform atomic_uint instance_count_lod_1;
layout(binding = 2) uniform atomic_uint instance_count_lod_2;
layout(binding = 3) uniform atomic_uint instance_count_lod_3;

layout(std430, binding = 2) buffer instance_id_out_lod_0
{
    writeonly uint data[];
} output_instance_id_lod_0;

layout(std430, binding = 3) buffer instance_id_out_lod_1
{
    writeonly uint data[];
} output_instance_id_lod_1;

layout(std430, binding = 4) buffer instance_id_out_lod_2
{
    writeonly uint data[];
} output_instance_id_lod_2;

layout(std430, binding = 5) buffer instance_id_out_lod_3
{
    writeonly uint data[];
} output_instance_id_lod_3;

// --------------------------------------------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------------------------------------------

void main()
{
    uint instance_id = gl_GlobalInvocationID.x;

    if(instance_id >= instance_count)
    {
        return;
    }

    if((input_bits_curr.data[instance_id/32] & (1u << (instance_id%32))) == 0)
    {
        return;
    }

    const Bound b = input_bound.data[instance_id];

    const float distance = -(camera.view_matrix * vec4((b.bmin.xyz + b.bmax.xyz) * 0.5f, 1.0f)).z;

    if(distance < 100.0f)
    {
        uint count = atomicCounterIncrement(instance_count_lod_0);
        output_instance_id_lod_0.data[count] = instance_id;
    }
    else if(distance < 200.0f)
    {
        uint count = atomicCounterIncrement(instance_count_lod_1);
        output_instance_id_lod_1.data[count] = instance_id;
    }
    else if(distance < 400.0f)
    {
        uint count = atomicCounterIncrement(instance_count_lod_2);
        output_instance_id_lod_2.data[count] = instance_id;
    }
    else
    {
        uint count = atomicCounterIncrement(instance_count_lod_3);
        output_instance_id_lod_3.data[count] = instance_id;
    }
}
