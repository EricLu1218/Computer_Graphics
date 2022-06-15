#version 330

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aTexCoord;

out vec3 vertex_pos;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec2 texCoord;

const float PI = 3.14159265358979323846;

uniform mat4 um4p;
uniform mat4 um4v;
uniform mat4 um4m;
uniform vec3 cameraPosition;

struct PhongMaterial
{
    vec3 Ka;
    vec3 Kd;
    vec3 Ks;
};
uniform PhongMaterial material;

struct LightInfo
{
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
};
uniform LightInfo lightInfo;

struct SpotLightInfo
{
    vec3 direction;
    float exponent;
    float cutoff;
};
uniform SpotLightInfo spotLightInfo;

uniform int curLightMode;
uniform float shininess;

// [TODO] passing uniform variable for texture coordinate offset

vec3 directionalLight(vec3 vertexPosition, vec3 vertexNormal)
{
    vec3 L = normalize(lightInfo.position);
    vec3 V = normalize(cameraPosition - vertexPosition);
    vec3 H = normalize(L + V);
    vec3 N = normalize(vertexNormal);

    vec3 ambient = lightInfo.ambient * material.Ka;
    vec3 diffuse = max(dot(L, N), 0.0f) * lightInfo.diffuse * material.Kd;
    vec3 specular = pow(max(dot(H, N), 0.0f), shininess) * lightInfo.specular * material.Ks;
    return ambient + diffuse + specular;
}

vec3 positionLight(vec3 vertexPosition, vec3 vertexNormal)
{
    vec3 L = normalize(lightInfo.position - vertexPosition);
    vec3 V = normalize(cameraPosition - vertexPosition);
    vec3 H = normalize(L + V);
    vec3 N = normalize(vertexNormal);

    vec3 ambient = lightInfo.ambient * material.Ka;
    vec3 diffuse = max(dot(L, N), 0.0f) * lightInfo.diffuse * material.Kd;
    vec3 specular = pow(max(dot(H, N), 0.0f), shininess) * lightInfo.specular * material.Ks;

    float dist = length(lightInfo.position - vertexPosition);
    float attenuation = lightInfo.attenuationConstant +
                        lightInfo.attenuationLinear * dist +
                        lightInfo.attenuationQuadratic * dist * dist;
    float f_att = min(1.0f / attenuation, 1.0f);
    return ambient + f_att * (diffuse + specular);
}

vec3 spotLight(vec3 vertexPosition, vec3 vertexNormal)
{
    vec3 L = normalize(lightInfo.position - vertexPosition);
    vec3 V = normalize(cameraPosition - vertexPosition);
    vec3 H = normalize(L + V);
    vec3 N = normalize(vertexNormal);

    vec3 ambient = lightInfo.ambient * material.Ka;
    vec3 diffuse = max(dot(L, N), 0.0f) * lightInfo.diffuse * material.Kd;
    vec3 specular = pow(max(dot(H, N), 0.0f), shininess) * lightInfo.specular * material.Ks;

    float dist = length(lightInfo.position - vertexPosition);
    float attenuation = lightInfo.attenuationConstant +
                        lightInfo.attenuationLinear * dist +
                        lightInfo.attenuationQuadratic * dist * dist;
    float f_att = min(1.0f / attenuation, 1.0f);

    vec3 v = normalize(vertexPosition - lightInfo.position);
    vec3 d = normalize(spotLightInfo.direction);
    float vd = dot(v, d);
    float spotEffect = 0;
    if (vd > cos(spotLightInfo.cutoff * PI / 180.0f))
        spotEffect = pow(max(vd, 0.0f), spotLightInfo.exponent);
    return ambient + spotEffect * f_att * (diffuse + specular);
}

void main()
{
    gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);

    vertex_pos = vec3(um4m * vec4(aPos, 1.0f));
    vertex_normal = mat3(transpose(inverse(um4m))) * aNormal;

    vec3 color;
    if (curLightMode == 0)
        color = directionalLight(vertex_pos, vertex_normal);
    else if (curLightMode == 1)
        color = positionLight(vertex_pos, vertex_normal);
    else if (curLightMode == 2)
        color = spotLight(vertex_pos, vertex_normal);
    vertex_color = color;

    // [TODO]
    texCoord = aTexCoord;
}
