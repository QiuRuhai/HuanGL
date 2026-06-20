#version 460 core
out vec4 FragColor;
uniform sampler2D uRealtime;   // resolved HDR
uniform sampler2D uReference;  // accumulation sum
uniform sampler2D uError;      // r=abs luma err, g=sq err, b=rel err
uniform vec2  uResolution;
uniform float uInvSampleCount;
uniform int   uView;           // 0 realtime, 1 reference, 2 split, 3 heatmap
uniform float uErrorScale;

vec3 tonemap(vec3 c){ c *= 1.0; return c / (c + vec3(1.0)); } // simple Reinhard for display
vec3 heat(float x){ // blue->green->red ramp
    x = clamp(x, 0.0, 1.0);
    return clamp(vec3(1.5 - abs(4.0*x - 3.0),
                      1.5 - abs(4.0*x - 2.0),
                      1.5 - abs(4.0*x - 1.0)), 0.0, 1.0);
}

void main(){
    vec2  uv = gl_FragCoord.xy / uResolution;
    ivec2 px = ivec2(gl_FragCoord.xy);
    vec3 rt = texelFetch(uRealtime,  px, 0).rgb;
    vec3 rf = texelFetch(uReference, px, 0).rgb * uInvSampleCount;
    vec3 outc;
    if      (uView == 0) outc = tonemap(rt);
    else if (uView == 1) outc = tonemap(rf);
    else if (uView == 2) outc = (uv.x < 0.5) ? tonemap(rt) : tonemap(rf);
    else { float e = texelFetch(uError, px, 0).r * uErrorScale; outc = heat(e); }
    FragColor = vec4(outc, 1.0);
}
