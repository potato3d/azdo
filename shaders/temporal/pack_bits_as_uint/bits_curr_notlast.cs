#version 450

// --------------------------------------------------------------------------------------------------------------
// INPUTS
// --------------------------------------------------------------------------------------------------------------

layout(local_size_x = 256) in;

layout(location = 0) uniform uint instance_count;

layout(std430, binding = 10) buffer visible_in
{
    readonly uint data[];
} input_visible;

layout(std430, binding = 12) buffer bits_last_in
{
    readonly uint data[];
} input_bits_last;

// --------------------------------------------------------------------------------------------------------------
// INPUTS
// --------------------------------------------------------------------------------------------------------------

layout(std430, binding = 11) buffer bits_curr_out
{
    writeonly uint data[];
} output_bits_curr;

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

    output_bits_curr.data[instance_id] = input_visible.data[instance_id] & (~input_bits_last.data[instance_id]);
}
