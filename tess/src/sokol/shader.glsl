// clang-format off
// clang-format doesn't handle the '@' directives.

@ctype mat4 rive::Mat4
@ctype vec2 rive::Vec2D

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = mvp * vec4(position.x, position.y, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs
uniform sampler2D tex;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(tex, uv);
}
@end

@program rive_tess vs fs


@vs vs_path
uniform vs_path_params {
    mat4 mvp;
    int fillType;
    vec2 gradientStart;
    vec2 gradientEnd;
};

in vec2 position;
out vec2 gradient_uv;

void main() {
    gl_Position =  mvp * vec4(position, 0.0, 1.0);
    
    if(fillType == 1) {
        // Linear gradient.
        vec2 toEnd = gradientEnd - gradientStart;
        float lengthSquared = toEnd.x * toEnd.x + toEnd.y * toEnd.y;
        gradient_uv.x = dot(position - gradientStart, toEnd) / lengthSquared;
    }
    else if(fillType == 2) {
        // fillType == 2 (Radial gradient)
        gradient_uv = (position - gradientStart) / distance(gradientStart, gradientEnd);
    }
}
@end

@fs fs_path

uniform fs_path_uniforms {
    int fillType;
    vec4 colors[16];
    vec4 stops[4];
    int stopCount;
};

in vec2 gradient_uv;
out vec4 frag_color;

void main() {
    if (fillType == 0) {
        // Solid color.
        frag_color = colors[0];
    }
    else {
        float f = fillType == 1 ? gradient_uv.x : length(gradient_uv);
        vec4 color =
            mix(colors[0], colors[1], smoothstep(stops[0][0], stops[0][1], f));
        for (int i = 1; i < 15; ++i)
        {
            if (i >= stopCount - 1)
            {
                break;
            }
            
            color = mix(color,
                            colors[i + 1],
                            smoothstep(stops[i/4][i%4], stops[(i+1)/4][(i+1)%4], f));
        }
        frag_color = color;
    }
}
@end

@program rive_tess_path vs_path fs_path
