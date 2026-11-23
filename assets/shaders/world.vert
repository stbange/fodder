layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aPlacement;
layout(location = 4) in vec4 aDye;
layout(location = 5) in float aPart;
layout(location = 6) in vec2 aPose;

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
            float load = smoothstep(0.0, 0.40, phase) * (1.0 - smoothstep(0.64, 1.0, phase));
            float slash = smoothstep(0.36, 0.64, phase);
            if (aPart == 1.0) {
                vec3 pivot = vec3(0.32, 1.16, -0.06);
                local -= pivot;
                local.xy = rotate(local.xy, load * (-0.90 + 2.65 * slash));
                local.yz = rotate(local.yz, load * (0.62 - 1.30 * slash));
                local += pivot;
            }
            local.z += load * (0.09 - 0.22 * slash) * local.y;
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
        if (aPart == 1.0 && aPose.x > 0.12 && aPose.x < 0.49) {
            vColor.rgb = mix(vColor.rgb, vec3(1.0, 0.81, 0.46), 0.32);
        }
    }
    vUV = aUV;
    vDistance = length(uEye - world.xyz);
    gl_Position = uCamera * world;
}
