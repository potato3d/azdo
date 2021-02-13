#version 450

in fdata
{
    vec3 eye_position;
    vec3 eye_normal;
} fin;

layout(location = 0) out vec4 output_color;

const vec3 DEFAULT_AMBIENT  = vec3(0.2f, 0.2f, 0.2f);
const vec3 DEFAULT_DIFFUSE  = vec3(0.6f, 0.6f, 0.6f);
const vec3 DEFAULT_SPECULAR = vec3(0.4f, 0.4f, 0.4f);
const float MATERIAL_SHININESS = 32.0f;
const float MATERIAL_ALPHA = 1.0f;

void main()
{
    const vec3 ambient = DEFAULT_AMBIENT;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    const vec3 n = normalize(fin.eye_normal);
    const vec3 l = normalize(-fin.eye_position); // light - vert

    float diffuseIntensity = dot(n,l);
    if(diffuseIntensity > 0.0f)
    {
        diffuse = DEFAULT_DIFFUSE * diffuseIntensity;

        const vec3 r = reflect(-l,n);
        const vec3 e = l; // cam - vert
        const float specularIntensity = max(dot(r,e), 0.0f);
        specular = DEFAULT_SPECULAR * pow(specularIntensity, MATERIAL_SHININESS);
    }

    output_color = vec4(ambient + diffuse + specular, MATERIAL_ALPHA);
}
