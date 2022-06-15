#version 330

in vec3 vertex_pos;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec2 texCoord;

out vec4 fragColor;

const float PI = 3.14159265358979323846;

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
uniform int isPerPixelLighting;

// [TODO] passing texture from main.cpp
// Hint: sampler2D
/* HW3 added */
uniform sampler2D diffuseTexture;
uniform int isEye;
uniform float offsetX;
uniform float offsetY;

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
    vec3 color;
    if (curLightMode == 0)
        color = directionalLight(vertex_pos, vertex_normal);
    else if (curLightMode == 1)
        color = positionLight(vertex_pos, vertex_normal);
    else if (curLightMode == 2)
        color = spotLight(vertex_pos, vertex_normal);
    fragColor = (isPerPixelLighting == 1) ? vec4(color, 1.0f) : vec4(vertex_color, 1.0f);

    // [TODO] sampleing from texture
    // Hint: texture
	/* HW3 added */
	if (isEye == 0)
    	fragColor *= texture(diffuseTexture, texCoord);
	else
		fragColor *= texture(diffuseTexture, texCoord + vec2(offsetX, offsetY));
}
