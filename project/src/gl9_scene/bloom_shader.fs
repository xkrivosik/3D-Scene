#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTex;
uniform sampler2D blurTex;
uniform bool useBlur;

// tone-mapping (Reinhard + gamma)
vec3 toneMap(vec3 hdr)
{
    hdr = hdr / (hdr + vec3(1.0));
    hdr = pow(hdr, vec3(2.0/2.2));
    return hdr;
}

void main()
{
    vec3 sceneColor = texture(sceneTex, TexCoords).rgb;
    vec3 blurred    = texture(blurTex, TexCoords).rgb;

    vec3 baseColor  = useBlur ? mix(sceneColor, blurred, 0.4) : sceneColor;
    vec3 bloom      = blurred * 0.15;

    vec3 finalColor = toneMap(baseColor + bloom);
    FragColor = vec4(finalColor, 1.0);
}
