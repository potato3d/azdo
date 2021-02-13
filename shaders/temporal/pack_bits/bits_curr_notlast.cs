#version 450

struct Flags
{
    uint data[32];
};

// --------------------------------------------------------------------------------------------------------------
// INPUTS
// --------------------------------------------------------------------------------------------------------------

layout(local_size_x = 256) in;

layout(location = 0) uniform uint instance_count;

layout(std430, binding = 10) buffer visible_in
{
    readonly Flags data[];
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

    if(instance_id > ceil(float(instance_count)/32.0f))
    {
        return;
    }

    uint bits = 0u;
    uint outbit = 0u;

    Flags flags = input_visible.data[instance_id];

    for(int i = 0; i < 32; ++i, ++outbit)
    {
        bits |= (flags.data[i] & 1u) << outbit;
    }

    output_bits_curr.data[instance_id] = bits & (~input_bits_last.data[instance_id]);
}
