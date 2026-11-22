layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aPlacement;
layout(location = 4) in vec4 aDye;

uniform mat4 uCamera;
uniform mat4 uModel;
uniform vec3 uEye;
uniform int uStyle;

out vec2 vUV;
out vec4 vColor;
out float vDistance;

void main()
{
    float s = sin(aPlacement.w);
    float c = cos(aPlacement.w);
    vec3 local = aPosition;
    if (aDye.a < 0.0) {
        local = vec3(local.x, 0.14 - local.z * 0.30, local.y - 0.40);
    }
    vec3 rotated = vec3(c * local.x - s * local.z, local.y, s * local.x + c * local.z);
    vec4 world = uModel * vec4(rotated + aPlacement.xyz, 1.0);
    if (uStyle == 3) {
        vColor = aColor;
    } else if (uStyle == 1) {
        vColor = aDye;
    } else if (uStyle == 2) {
        vColor = aColor;
    } else {
        vColor = vec4(mix(aColor.rgb, aColor.rgb * aDye.rgb, aColor.a), 1.0);
    }
    vUV = aUV;
    vDistance = length(uEye - world.xyz);
    gl_Position = uCamera * world;
}
