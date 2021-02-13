#version 450

struct Bound
{
    vec4 bmin;
    vec4 bmax;
};

layout(points, invocations = 6) in;
layout(triangle_strip, max_vertices = 4) out;

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

in vdata
{
    uint instance_id;
} vin[];

out fdata
{
    flat uint instance_id;
} fout;

void main()
{
    uint instance_id = vin[0].instance_id;
    Bound bound = input_bound.data[instance_id];

    switch(gl_InvocationID)
    {
    case 0:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    case 1:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    case 2:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    case 3:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    case 4:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmax.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    case 5:
        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();

        gl_Position = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmin.z, 1.0f);
        fout.instance_id = instance_id;
        EmitVertex();
        break;
    }

    EndPrimitive();
}
