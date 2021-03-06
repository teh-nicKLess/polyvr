#version 120

#extension GL_ARB_texture_rectangle : require
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect texBufPos;
uniform sampler2DRect texBufNorm;
uniform sampler2DRect texBufDiff;
uniform vec2          vpOffset;
uniform int           channel;
uniform float shadowColor = 0.0;

uniform vec3 lightUp;
uniform vec3 lightDir;
uniform vec3 lightPos;

vec3 pos;
vec4 norm;
vec4 color = vec4(0);

vec4 OSG_SSME_FP_calcShadow(in vec4 ecFragPos);

void computePointLight() {
    vec3  lightDirUN = lightPos - pos;
    vec3  lightDir   = normalize(lightDirUN);
    float NdotL      = max(dot(norm.xyz, lightDir), 0.);

    if(NdotL > 0.) {
        vec4  shadow    = OSG_SSME_FP_calcShadow(vec4(pos, 1.));
        shadow += shadowColor;

        float lightDist = length(lightDirUN);
        float distAtt   = dot(vec3(gl_LightSource[0].constantAttenuation,
                                   gl_LightSource[0].linearAttenuation,
                                   gl_LightSource[0].quadraticAttenuation),
                              vec3(1., lightDist, lightDist * lightDist));
        distAtt = 1. / distAtt;
        color = shadow * distAtt * NdotL * color * gl_LightSource[0].diffuse;
    } else color = vec4(0);
}

void main(void) {
    vec2 lookup = gl_FragCoord.xy - vpOffset;
    norm = texture2DRect(texBufNorm, lookup);
    bool isLit = (norm.w > 0);

    if(channel != 0 || !isLit || dot(norm.xyz, norm.xyz) < 0.95) discard;
    else {
        vec4 posAmb = texture2DRect(texBufPos,  lookup);
        pos = posAmb.xyz;
        color = texture2DRect(texBufDiff, lookup);
	computePointLight();
        gl_FragColor = color;
    }
}
