#version 330 core
out vec4 FragColor;
uniform float u_time;
uniform vec2 u_resolution;

// Clamp zoom range for persistent fractals
float get_zoom(float t) {
    float min_zoom = 0.7;
    float max_zoom = 1.8;
    float period = 24.0; // seconds for a full cycle
    float phase = mod(u_time, period) / period;
    float oscillation = 0.5 * (1.0 - cos(6.2831 * phase));
    return mix(min_zoom, max_zoom, oscillation);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    float t = u_time * 0.3;
    float zoom = get_zoom(u_time);
    vec2 c = vec2(-0.70176 + 0.25 * cos(t * 1.1), -0.3842 + 0.25 * sin(t * 0.9));
    vec2 z = uv / zoom;
    int max_iter = 120;
    int i;
    for (i = 0; i < max_iter; ++i) {
        float x = (z.x * z.x - z.y * z.y) + c.x;
        float y = (2.0 * z.x * z.y) + c.y;
        z = vec2(x, y);
        if (dot(z, z) > 4.0) break;
    }
    float norm = float(i) / float(max_iter);
    float hue = mod(norm + 0.5 * cos(u_time * 0.07), 1.0);
    float sat = 0.8 + 0.2 * sin(u_time * 0.2);
    float val = norm < 1.0 ? 1.0 : 0.0;
    // HSV to RGB
    float c_ = val * sat;
    float h_ = hue * 6.0;
    float x_ = c_ * (1.0 - abs(mod(h_, 2.0) - 1.0));
    vec3 rgb;
    if (h_ < 1.0)      rgb = vec3(c_, x_, 0.0);
    else if (h_ < 2.0) rgb = vec3(x_, c_, 0.0);
    else if (h_ < 3.0) rgb = vec3(0.0, c_, x_);
    else if (h_ < 4.0) rgb = vec3(0.0, x_, c_);
    else if (h_ < 5.0) rgb = vec3(x_, 0.0, c_);
    else               rgb = vec3(c_, 0.0, x_);
    float m = val - c_;
    rgb += vec3(m);
    FragColor = vec4(rgb, 1.0);
}
