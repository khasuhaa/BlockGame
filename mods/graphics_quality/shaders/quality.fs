#version 330
// Graphics Quality: the world as drawn, with light that spills from what is bright, a little more colour and
// contrast, a touch of warmth, and the corners drawn in. How much of each is in the mod's rules.json.
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloom, vignette, saturation, contrast, warmth;
out vec4 finalColor;

void main()
{
    vec3 c = texture(texture0, fragTexCoord).rgb;
    vec2 px = 1.0 / resolution;
    vec3 glow = vec3(0.0);
    float total = 0.0;
    for (int i = -3; i <= 3; i++)
        for (int j = -3; j <= 3; j++) {
            vec3 s = texture(texture0, fragTexCoord + vec2(float(i), float(j)) * px * 3.0).rgb;
            float w = exp(-float(i * i + j * j) / 8.0);
            // (Only what is brighter than a clear sky spills: the sun, a flame, a lamp.)
            glow += s * smoothstep(0.80, 1.0, dot(s, vec3(0.299, 0.587, 0.114))) * w;
            total += w;
        }
    c += glow / total * bloom * 1.1;
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, saturation);
    c = (c - 0.5) * contrast + 0.5;
    c += vec3(warmth, warmth * 0.3, -warmth);
    c *= 1.0 - vignette * smoothstep(0.35, 0.85, length(fragTexCoord - 0.5) * 1.25);
    // (A grain too fine to see, so that the sky's slow fade does not show as bands.)
    float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    c += (n - 0.5) / 255.0;
    finalColor = vec4(clamp(c, 0.0, 1.0), 1.0) * colDiffuse * fragColor;
}
