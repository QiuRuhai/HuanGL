#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;
uniform vec2 uTexelSize;
uniform bool uFirstPass;

// Karis average: suppresses firefly pixels by down-weighting bright
// outliers.  Applied per 2x2 group on the first downsample pass only,
// where the HDR dynamic range is highest.
float KarisWeight(vec3 c) {
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return 1.0 / (1.0 + luma);
}

// 13-tap downsample filter (Jimenez 2014, Call of Duty: Advanced Warfare).
//
// Tap layout (offsets in texels from center):
//
//   a(-1,-1)  b( 0,-1)  c(+1,-1)
//        d(-½,-½)  e(+½,-½)
//   f(-1, 0)  g( 0, 0)  h(+1, 0)
//        i(-½,+½)  j(+½,+½)
//   k(-1,+1)  l( 0,+1)  m(+1,+1)
//
// Five overlapping 2x2 groups:
//   Center (d,e,i,j)  weight 0.500
//   TL (a,b,f,g)      weight 0.125
//   TR (b,c,g,h)      weight 0.125
//   BL (f,g,k,l)      weight 0.125
//   BR (g,h,l,m)      weight 0.125
//
void main() {
    vec2 t = uTexelSize;

    // Inner 4 samples at half-texel offsets
    vec3 d = texture(uInput, vUV + vec2(-t.x, -t.y) * 0.5).rgb;
    vec3 e = texture(uInput, vUV + vec2( t.x, -t.y) * 0.5).rgb;
    vec3 i = texture(uInput, vUV + vec2(-t.x,  t.y) * 0.5).rgb;
    vec3 j = texture(uInput, vUV + vec2( t.x,  t.y) * 0.5).rgb;

    // Outer 9 samples at full-texel offsets
    vec3 a = texture(uInput, vUV + vec2(-t.x, -t.y)).rgb;
    vec3 b = texture(uInput, vUV + vec2( 0.0, -t.y)).rgb;
    vec3 c = texture(uInput, vUV + vec2( t.x, -t.y)).rgb;
    vec3 f = texture(uInput, vUV + vec2(-t.x,  0.0)).rgb;
    vec3 g = texture(uInput, vUV).rgb;
    vec3 h = texture(uInput, vUV + vec2( t.x,  0.0)).rgb;
    vec3 k = texture(uInput, vUV + vec2(-t.x,  t.y)).rgb;
    vec3 l = texture(uInput, vUV + vec2( 0.0,  t.y)).rgb;
    vec3 m = texture(uInput, vUV + vec2( t.x,  t.y)).rgb;

    if (uFirstPass) {
        // Karis average: average each 2x2 group first, then weight by
        // inverse luminance so bright outliers cannot dominate.
        vec3 grpC  = (d + e + i + j) * 0.25;
        vec3 grpTL = (a + b + f + g) * 0.25;
        vec3 grpTR = (b + c + g + h) * 0.25;
        vec3 grpBL = (f + g + k + l) * 0.25;
        vec3 grpBR = (g + h + l + m) * 0.25;

        float wC  = KarisWeight(grpC);
        float wTL = KarisWeight(grpTL);
        float wTR = KarisWeight(grpTR);
        float wBL = KarisWeight(grpBL);
        float wBR = KarisWeight(grpBR);

        // Weighted combination preserving the group weight ratio (0.5 / 0.125).
        float sumW = wC * 0.5 + (wTL + wTR + wBL + wBR) * 0.125;
        FragColor = vec4((grpC * wC * 0.5 +
                          grpTL * wTL * 0.125 +
                          grpTR * wTR * 0.125 +
                          grpBL * wBL * 0.125 +
                          grpBR * wBR * 0.125) / max(sumW, 1e-5), 1.0);
    } else {
        // Subsequent passes: standard weighted 13-tap without Karis
        // (dynamic range is already reduced by prior downsamples).
        //
        // Per-sample effective weights (from the 5-group decomposition):
        //   corners a,c,k,m  = 0.03125 each  (1 group)
        //   edges   b,f,h,l  = 0.0625  each  (2 groups)
        //   center  g        = 0.125          (4 groups)
        //   inner   d,e,i,j  = 0.125   each  (center group)
        vec3 color;
        color  = g * 0.125;
        color += (d + e + i + j) * 0.125;
        color += (b + f + h + l) * 0.0625;
        color += (a + c + k + m) * 0.03125;
        FragColor = vec4(color, 1.0);
    }
}
