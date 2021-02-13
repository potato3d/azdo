#version 450

layout (points) in;
layout (points, max_vertices = 1) out;

layout(std140) uniform cdata
{
    mat4 view_proj_matrix;
    mat4 normal_matrix;
    mat4 view_matrix;
} camera;

in vdata
{
    bool visible;
    vec3 position;
    vec4 transform_0;
    vec4 transform_1;
    vec4 transform_2;
} vin[];

layout(stream = 0) out fdata0
{
    vec4 transform_0;
    vec4 transform_1;
    vec4 transform_2;
} vout0;

layout(stream = 1) out fdata1
{
    vec4 transform_0;
    vec4 transform_1;
    vec4 transform_2;
} vout1;

layout(stream = 2) out fdata2
{
    vec4 transform_0;
    vec4 transform_1;
    vec4 transform_2;
} vout2;

void main()
{
    if(vin[0].visible)
    {
        float distance = -(camera.view_matrix * vec4(vin[0].position, 1.0f)).z;

        if(distance < 100)
        {
            vout0.transform_0 = vin[0].transform_0;
            vout0.transform_1 = vin[0].transform_1;
            vout0.transform_2 = vin[0].transform_2;
            EmitStreamVertex(0);
        }
        else if(distance < 500)
        {
            vout1.transform_0 = vin[0].transform_0;
            vout1.transform_1 = vin[0].transform_1;
            vout1.transform_2 = vin[0].transform_2;
            EmitStreamVertex(1);
        }
        else
        {
            vout2.transform_0 = vin[0].transform_0;
            vout2.transform_1 = vin[0].transform_1;
            vout2.transform_2 = vin[0].transform_2;
            EmitStreamVertex(2);
        }
    }
}
