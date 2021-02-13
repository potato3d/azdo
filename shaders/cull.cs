#version 450

struct Transform
{
    vec4 m0;
    vec4 m1;
    vec4 m2;
};

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

layout(binding = 0) uniform sampler2D depth_texture;

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
// AUXILIARY FUNCTIONS
// --------------------------------------------------------------------------------------------------------------

bool visible(const Bound bound)
{
    // ------------------------------------------------------------------------------
    // frustum culling
    // ------------------------------------------------------------------------------

    // tests *must* be done in clip space, *not* NDC space

    // clip-space bounding box
    vec4 bbox[8];
    bbox[0] = camera.view_proj_matrix * bound.bmax;
    bbox[1] = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmax.z, 1.0f);
    bbox[2] = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmax.z, 1.0f);
    bbox[3] = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmin.y, bound.bmax.z, 1.0f);
    bbox[4] = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmax.y, bound.bmin.z, 1.0f);
    bbox[5] = camera.view_proj_matrix * vec4(bound.bmin.x, bound.bmax.y, bound.bmin.z, 1.0f);
    bbox[6] = camera.view_proj_matrix * vec4(bound.bmax.x, bound.bmin.y, bound.bmin.z, 1.0f);
    bbox[7] = camera.view_proj_matrix * bound.bmin;

    // count how many vertices are outside each frustum plane
    int numOutside[6] = int[6](0,0,0,0,0,0);

    for(int i = 0; i < 8; ++i)
    {
        if(bbox[i].x >  bbox[i].w) ++numOutside[0];
        if(bbox[i].x < -bbox[i].w) ++numOutside[1];
        if(bbox[i].y >  bbox[i].w) ++numOutside[2];
        if(bbox[i].y < -bbox[i].w) ++numOutside[3];
        if(bbox[i].z >  bbox[i].w) ++numOutside[4];
        if(bbox[i].z < -bbox[i].w) ++numOutside[5];
    }

    // if all vertices are outside at least one frustum plane, discard
    for(int i = 0; i < 6; ++i)
    {
        if(numOutside[i] == 8)
        {
            return false;
        }
    }

    // ------------------------------------------------------------------------------
    // occlusion culling
    // ------------------------------------------------------------------------------

    // if bounding box crosses near-plane, consider visible
    if(numOutside[5] > 0)
    {
        return true;
    }

    // convert to NDC coordinates
    vec3 ndc_min = bbox[0].xyz / bbox[0].w;
    vec3 ndc_max = ndc_min;
    for(int i = 1; i < 8; ++i)
    {
        ndc_min = min(ndc_min, bbox[i].xyz / bbox[i].w);
        ndc_max = max(ndc_max, bbox[i].xyz / bbox[i].w);
    }

    ndc_min = ndc_min * 0.5 + 0.5;
    ndc_max = ndc_max * 0.5 + 0.5;

    // compute screen size in pixels
    vec2 size = (ndc_max.xy - ndc_min.xy);
    ivec2 texsize = textureSize(depth_texture,0);
    float maxsize = max(size.x, size.y) * float(max(texsize.x,texsize.y));

    // small-feature culling
    if(maxsize <= 1.0f)
    {
        return false;
    }

    // compute correct hi-z mipmap level
    float miplevel = ceil(log2(maxsize));

    // fetch 4 hi-z depths that cover screen-space bounding box
    float depth = 0.0f;
    float a = textureLod(depth_texture,ndc_min.xy,miplevel).r;
    float b = textureLod(depth_texture,vec2(ndc_max.x,ndc_min.y),miplevel).r;
    float c = textureLod(depth_texture,ndc_max.xy,miplevel).r;
    float d = textureLod(depth_texture,vec2(ndc_min.x,ndc_max.y),miplevel).r;
    depth = max(depth,max(max(max(a,b),c),d));

    return ndc_min.z < depth;
}

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

    const Bound b = input_bound.data[instance_id];

    if(!visible(b))
    {
        return;
    }

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
