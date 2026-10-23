struct Uniforms {
    tint: vec4<f32>,
    offset: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) shade: f32,
};

@vertex
fn vs_main(@location(0) position: vec2<f32>, @location(1) shade: f32) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4<f32>(position + u.offset.xy, 0.0, 1.0);
    out.shade = shade;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return vec4<f32>(u.tint.rgb * in.shade, 1.0);
}
