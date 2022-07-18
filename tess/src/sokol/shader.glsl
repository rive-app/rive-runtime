@ctype mat4 rive::Mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec2 pos;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = mvp * vec4(pos.x, pos.y, 0.0, 1.0);
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