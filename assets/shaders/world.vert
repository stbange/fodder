layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aPlacement;
layout(location = 4) in vec4 aDye;
layout(location = 5) in float aPart;
layout(location = 6) in vec4 aPose;

uniform mat4 uCamera;
uniform mat4 uModel;
uniform vec3 uEye;
uniform int uStyle;

out vec2 vUV;
out vec4 vColor;
out float vDistance;

vec2 rotate(vec2 point, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec2(c * point.x - s * point.y, s * point.x + c * point.y);
}

void main()
{
    float s = sin(aPlacement.w);
    float c = cos(aPlacement.w);
    vec3 local = aPosition;
    if (aDye.a < 0.0) {
        local = vec3(local.x, 0.14 - local.z * 0.30, local.y - 0.40);
    } else if (uStyle == 0) {
        float phase = aPose.x;
        if (phase > 0.0) {
            float load = smoothstep(0.0, 0.27, phase) * (1.0 - smoothstep(0.64, 1.0, phase));
            float swing = smoothstep(0.30, 0.55, phase);
            if (aPart == 1.0) {
                vec3 pivot = vec3(0.32, 1.16, -0.06);
                local -= pivot;
                if (aPose.z > 0.0) {
                    local.yz = rotate(local.yz, load * (0.94 - swing * 2.64));
                    local.y += load * (0.39 - swing * 0.46);
                } else {
                    local.xy = rotate(local.xy, load * (-1.26 + 2.90 * swing));
                    local.yz = rotate(local.yz, load * (0.46 - 0.84 * swing));
                }
                local += pivot;
            }
            local.z += load * (0.09 - 0.22 * swing) * local.y;
        } else if (aPart == 2.0 && aPose.y > 0.0) {
            local.y += 0.22;
            local.x += 0.09;
            local.z -= 0.10;
        }
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
        if (aPose.w > 0.0) {
            vColor.rgb = mix(vColor.rgb, vec3(1.0, 0.79, 0.39), 0.45);
        }
        if (aPart == 1.0 && aPose.x > 0.12 && aPose.x < 0.49) {
            vColor.rgb = mix(vColor.rgb, vec3(1.0, 0.81, 0.46), 0.32);
        }
    }
    vUV = aUV;
    vDistance = length(uEye - world.xyz);
    gl_Position = uCamera * world;
}
