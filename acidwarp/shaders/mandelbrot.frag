#version 330 core
out vec4 FragColor;
uniform float u_time;
uniform vec2 u_resolution;
uniform float u_zoom;
uniform float u_swirl;
uniform vec2 u_center;

// DOUBLE PRECISION Mandelbrot for deep zoom
#extension GL_ARB_gpu_shader_fp64 : enable

// Three-Color Gradient Palette
vec3 palette3(float t, vec3 a, vec3 b, vec3 c) {
    // t in [0,1]; a, b, c are colors
    if (t < 0.5) {
        float f = t * 2.0;
        return mix(a, b, f);
    } else {
        float f = (t - 0.5) * 2.0;
        return mix(b, c, f);
    }
}

void main() {
    // Convert uniforms to double
    dvec2 u_center64 = dvec2(u_center);
    double u_zoom64 = double(u_zoom);
    dvec2 uv = (gl_FragCoord.xy / u_resolution) * 2.0 - 1.0;
    uv.x *= double(u_resolution.x) / double(u_resolution.y);
    double scale = 1.5 / u_zoom64;
    // Swirl
    double cs = double(cos(float(u_swirl)));
    double sn = double(sin(float(u_swirl)));
    dvec2 z = dvec2(cs*uv.x - sn*uv.y, sn*uv.x + cs*uv.y) * scale;
    dvec2 c = u_center64 + z;
    dvec2 z0 = dvec2(0.0);
    int max_iter = 400;
    int iter = 0;
    for (int i = 0; i < max_iter; ++i) {
        double x = (z0.x * z0.x - z0.y * z0.y) + c.x;
        double y = (2.0 * z0.x * z0.y) + c.y;
        z0 = dvec2(x, y);
        if (dot(z0, z0) > 4.0) {
            iter = i;
            break;
        }
    }
    double mu = double(iter) - double(log2(float(log2(float(dot(z0, z0))))));
    double norm = mu / double(max_iter);
    // Three-Color Gradient
    vec3 color_a = vec3(0.1, 0.2, 0.8); // blue
    vec3 color_b = vec3(0.9, 0.8, 0.2); // yellow
    vec3 color_c = vec3(0.8, 0.1, 0.2); // red
    float color_cycle = mod(u_time * 0.045, 1.0);
    float t = mod(float(norm) + color_cycle, 1.0);
    float val = (iter < max_iter) ? 1.0 : 0.15;
    vec3 rgb = palette3(t, color_a, color_b, color_c) * val;
    FragColor = vec4(rgb, 1.0);
}
