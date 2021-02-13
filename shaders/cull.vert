#version 450

layout(std140) uniform cdata
{
    mat4 view_proj_matrix;
    mat4 normal_matrix;
    mat4 view_matrix;
} camera;

in vec3 in_bmin;
in vec3 in_bmax;
in vec4 in_transform_0;
in vec4 in_transform_1;
in vec4 in_transform_2;

out vdata
{
    bool visible;
    vec3 position;
    vec4 transform_0;
    vec4 transform_1;
    vec4 transform_2;
} vout;

bool visible_frustum()
{
    vec4 bbox[8];
    bbox[0] = camera.view_proj_matrix * vec4(in_bmax, 1.0f);
    bbox[1] = camera.view_proj_matrix * vec4(in_bmin.x, in_bmax.y, in_bmax.z, 1.0f);
    bbox[2] = camera.view_proj_matrix * vec4(in_bmax.x, in_bmin.y, in_bmax.z, 1.0f);
    bbox[3] = camera.view_proj_matrix * vec4(in_bmin.x, in_bmin.y, in_bmax.z, 1.0f);
    bbox[4] = camera.view_proj_matrix * vec4(in_bmax.x, in_bmax.y, in_bmin.z, 1.0f);
    bbox[5] = camera.view_proj_matrix * vec4(in_bmin.x, in_bmax.y, in_bmin.z, 1.0f);
    bbox[6] = camera.view_proj_matrix * vec4(in_bmax.x, in_bmin.y, in_bmin.z, 1.0f);
    bbox[7] = camera.view_proj_matrix * vec4(in_bmin, 1.0f);

    int outOfBound[6] = int[6](0, 0, 0, 0, 0, 0);

    for(int i = 0; i < 8; ++i)
    {
        if(bbox[i].x >  bbox[i].w) outOfBound[0]++;
        if(bbox[i].x < -bbox[i].w) outOfBound[1]++;
        if(bbox[i].y >  bbox[i].w) outOfBound[2]++;
        if(bbox[i].y < -bbox[i].w) outOfBound[3]++;
        if(bbox[i].z >  bbox[i].w) outOfBound[4]++;
        if(bbox[i].z < -bbox[i].w) outOfBound[5]++;
    }

    for(int i = 0; i < 6; ++i)
    {
        if(outOfBound[i] == 8)
        {
            return false;
        }
    }

    return true;
}

void main()
{
    vout.visible = visible_frustum();
    vout.position = (in_bmin + in_bmax) * 0.5f;
    vout.transform_0 = in_transform_0;
    vout.transform_1 = in_transform_1;
    vout.transform_2 = in_transform_2;
}
