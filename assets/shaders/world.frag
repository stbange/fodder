in vec2 vUV;
in vec4 vColor;
in float vDistance;

uniform sampler2D uTexture;
uniform int uStyle;
uniform float uFog;
uniform vec3 uHaze;
out vec4 fragmentColor;

void main()
{
    vec4 pixel = vColor;
    if (uStyle == 1 || uStyle == 2) {
        vec4 texel = texture(uTexture, vUV);
        if (texel.a < 0.60) {
            discard;
        }
        pixel = vec4(mix(texel.rgb * 0.88, texel.rgb * vColor.rgb, 0.78), 1.0);
    }
    if (uFog > 0.0 && uStyle != 3) {
        float fog = smoothstep(uFog * 0.25, uFog, vDistance);
        pixel.rgb = mix(pixel.rgb, uHaze, fog * 0.94);
    }
    fragmentColor = pixel;
}
