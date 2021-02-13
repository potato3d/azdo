#version 450

layout(early_fragment_tests) in;

in fdata
{
    flat uint instance_id;
} fin;

layout(std430, binding = 10) buffer visible_in
{
    writeonly uint data[];
} output_visible;

void main ()
{
    output_visible.data[fin.instance_id] = 1;
}
