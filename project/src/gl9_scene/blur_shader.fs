#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float offset = 1.0 / 500.0; // veľkosť posunu pre kernel

void main() {
    vec3 result = vec3(0.0); // akumulácia farby

    // jednoduchý 3×3 blur
    for(int x = -1; x <= 1; ++x)
        for(int y = -1; y <= 1; ++y)
            result += texture(screenTexture,
                              TexCoords + vec2(x, y) * offset).rgb / 6.0;

    FragColor = vec4(result, 1.0);
}
