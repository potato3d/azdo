#version 450

out vdata
{
    uint instance_id;
} vout;

void main()
{
    vout.instance_id = gl_VertexID;
}
