#![allow(clippy::missing_safety_doc)]

use core::slice;
use std::{io::Cursor, ptr::NonNull};

use image::io::Reader;
use smallvec::SmallVec;
use util::{ScaleFromOrigin, UnwrapAndDeref};
use vello::{
    kurbo::{Affine, BezPath, Line, PathSeg, Point, Rect, Shape, Vec2},
    peniko::{
        BlendMode, Brush, BrushRef, Cap, Color, ColorStop, ColorStopsSource, Fill, Format,
        Gradient, Image, Join, Mix, Stroke,
    },
    SceneBuilder, SceneFragment,
};

mod rive;
mod util;
mod viewer;

pub use viewer::ViewerContent;

fn from_bgra8(color: u32) -> Color {
    Color::rgba8(
        (color >> 16) as u8,
        (color >> 8) as u8,
        color as u8,
        (color >> 24) as u8,
    )
}

#[derive(Debug)]
enum RenderStyle {
    Fill,
    Stroke(Stroke),
}

#[derive(Debug)]
struct SliceStops<'s> {
    colors: &'s [u32],
    stops: &'s [f32],
}

impl ColorStopsSource for SliceStops<'_> {
    fn collect_stops(&self, vec: &mut SmallVec<[ColorStop; 4]>) {
        vec.extend(
            self.colors
                .iter()
                .zip(self.stops.iter())
                .map(|(&color, &offset)| ColorStop {
                    offset,
                    color: from_bgra8(color),
                }),
        );
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_gradient_new_linear(
    sx: f32,
    sy: f32,
    ex: f32,
    ey: f32,
    colors_data: *const u32,
    stops_data: *const f32,
    len: usize,
) -> Option<NonNull<Gradient>> {
    let colors = slice::from_raw_parts(colors_data, len);
    let stops = slice::from_raw_parts(stops_data, len);

    let raw_stops = SliceStops { colors, stops };
    let gradient =
        Gradient::new_linear((sx as f64, sy as f64), (ex as f64, ey as f64)).with_stops(raw_stops);

    NonNull::new(Box::into_raw(Box::new(gradient)))
}

#[no_mangle]
pub unsafe extern "C" fn vello_gradient_new_radial(
    cx: f32,
    cy: f32,
    radius: f32,
    colors_data: *const u32,
    stops_data: *const f32,
    len: usize,
) -> Option<NonNull<Gradient>> {
    let colors = slice::from_raw_parts(colors_data, len);
    let stops = slice::from_raw_parts(stops_data, len);

    let raw_stops = SliceStops { colors, stops };
    let gradient = Gradient::new_radial((cx as f64, cy as f64), radius).with_stops(raw_stops);

    NonNull::new(Box::into_raw(Box::new(gradient)))
}

#[no_mangle]
pub unsafe extern "C" fn vello_gradient_release(gradient: Option<NonNull<Gradient>>) {
    gradient.map(|ptr| Box::from_raw(ptr.as_ptr())).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn vello_image_new(data: *const u8, len: usize) -> Option<NonNull<Image>> {
    let image = Reader::new(Cursor::new(slice::from_raw_parts(data, len)))
        .with_guessed_format()
        .ok()?
        .decode()
        .ok()?
        .into_rgba8();
    let width = image.width();
    let height = image.height();

    NonNull::new(Box::into_raw(Box::new(Image::new(
        image.into_raw().into(),
        Format::Rgba8,
        width,
        height,
    ))))
}

#[no_mangle]
pub unsafe extern "C" fn vello_image_release(image: Option<NonNull<Image>>) {
    image.map(|ptr| Box::from_raw(ptr.as_ptr())).unwrap();
}

#[derive(Debug)]
pub struct VelloPaint {
    style: RenderStyle,
    brush: Brush,
    blend_mode: BlendMode,
}

impl Default for VelloPaint {
    fn default() -> Self {
        Self {
            style: RenderStyle::Fill,
            brush: Brush::Solid(Color::TRANSPARENT),
            blend_mode: Mix::Normal.into(),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_new() -> Option<NonNull<VelloPaint>> {
    NonNull::new(Box::into_raw(Box::default()))
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_release(paint: Option<NonNull<VelloPaint>>) {
    paint.map(|ptr| Box::from_raw(ptr.as_ptr())).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_style(
    mut paint: Option<NonNull<VelloPaint>>,
    style: rive::RenderPaint,
) {
    paint.unwrap_and_deref().style = style.into();
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_color(mut paint: Option<NonNull<VelloPaint>>, color: u32) {
    paint.unwrap_and_deref().brush = Brush::Solid(from_bgra8(color));
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_gradient(
    mut paint: Option<NonNull<VelloPaint>>,
    mut gradient: Option<NonNull<Gradient>>,
) {
    paint.unwrap_and_deref().brush = Brush::Gradient(gradient.unwrap_and_deref().clone());
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_thickness(
    mut paint: Option<NonNull<VelloPaint>>,
    thickness: f32,
) {
    let style = &mut paint.unwrap_and_deref().style;
    loop {
        if let RenderStyle::Stroke(stroke) = style {
            stroke.width = thickness;
            break;
        } else {
            *style = RenderStyle::Stroke(Stroke::new(0.0));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_join(
    mut paint: Option<NonNull<VelloPaint>>,
    join: rive::StrokeJoin,
) {
    let style = &mut paint.unwrap_and_deref().style;
    loop {
        if let RenderStyle::Stroke(stroke) = style {
            stroke.join = match join {
                rive::StrokeJoin::Miter => Join::Miter,
                rive::StrokeJoin::Round => Join::Round,
                rive::StrokeJoin::Bevel => Join::Bevel,
            };
            break;
        } else {
            *style = RenderStyle::Stroke(Stroke::new(0.0));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_cap(
    mut paint: Option<NonNull<VelloPaint>>,
    cap: rive::StrokeCap,
) {
    let style = &mut paint.unwrap_and_deref().style;
    loop {
        if let RenderStyle::Stroke(stroke) = style {
            stroke.start_cap = match cap {
                rive::StrokeCap::Butt => Cap::Butt,
                rive::StrokeCap::Round => Cap::Round,
                rive::StrokeCap::Square => Cap::Square,
            };
            stroke.end_cap = stroke.start_cap;
            break;
        } else {
            *style = RenderStyle::Stroke(Stroke::new(0.0));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_paint_set_blend_mode(
    mut paint: Option<NonNull<VelloPaint>>,
    blend_mode: rive::BlendMode,
) {
    let mix: Mix = blend_mode.into();
    paint.unwrap_and_deref().blend_mode = mix.into();
}

#[derive(Debug)]
pub struct VelloPath {
    path: BezPath,
    fill: Fill,
}

impl Default for VelloPath {
    fn default() -> Self {
        Self {
            path: Default::default(),
            fill: Fill::NonZero,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_new() -> Option<NonNull<VelloPath>> {
    NonNull::new(Box::into_raw(Box::default()))
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_release(path: Option<NonNull<VelloPath>>) {
    path.map(|ptr| Box::from_raw(ptr.as_ptr())).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_set_fill_rule(
    mut path: Option<NonNull<VelloPath>>,
    fill_rule: rive::FillRule,
) {
    path.unwrap_and_deref().fill = match fill_rule {
        rive::FillRule::NonZero => Fill::NonZero,
        rive::FillRule::EvenOdd => Fill::EvenOdd,
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_rewind(mut path: Option<NonNull<VelloPath>>) {
    path.unwrap_and_deref().path.truncate(0);
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_extend(
    mut path: Option<NonNull<VelloPath>>,
    mut from: Option<NonNull<VelloPath>>,
    mut transform: Option<NonNull<[f32; 6]>>,
) {
    let mut from = from.unwrap_and_deref().path.clone();
    from.apply_affine(Affine::new(transform.unwrap_and_deref().map(Into::into)));

    path.unwrap_and_deref()
        .path
        .extend(from.elements().iter().cloned());
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_move_to(mut path: Option<NonNull<VelloPath>>, x: f32, y: f32) {
    path.unwrap_and_deref()
        .path
        .move_to(Point::new(x as f64, y as f64));
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_line_to(mut path: Option<NonNull<VelloPath>>, x: f32, y: f32) {
    path.unwrap_and_deref()
        .path
        .line_to(Point::new(x as f64, y as f64));
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_cubic_to(
    mut path: Option<NonNull<VelloPath>>,
    ox: f32,
    oy: f32,
    ix: f32,
    iy: f32,
    x: f32,
    y: f32,
) {
    path.unwrap_and_deref().path.curve_to(
        Point::new(ox as f64, oy as f64),
        Point::new(ix as f64, iy as f64),
        Point::new(x as f64, y as f64),
    );
}

#[no_mangle]
pub unsafe extern "C" fn vello_path_close(mut path: Option<NonNull<VelloPath>>) {
    path.unwrap_and_deref().path.close_path();
}

pub struct VelloRenderer {
    pub scene: Box<SceneFragment>,
    builder: SceneBuilder<'static>,
    transforms: Vec<Affine>,
    clips: Vec<bool>,
}

impl VelloRenderer {
    fn last_transform(&mut self) -> &mut Affine {
        self.transforms.last_mut().unwrap()
    }

    fn last_clip(&mut self) -> &mut bool {
        self.clips.last_mut().unwrap()
    }
}

impl Default for VelloRenderer {
    fn default() -> Self {
        let mut scene = Box::<SceneFragment>::default();
        let builder = {
            let scene_mut: &mut SceneFragment = &mut scene;
            SceneBuilder::for_fragment(unsafe {
                // Quite a hack until we have a better way to do this in Vello.
                // Pretend that the scene fragment pointer lives for 'static.
                std::mem::transmute(scene_mut)
            })
        };

        Self {
            scene,
            builder,
            transforms: vec![Affine::IDENTITY],
            clips: vec![false],
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_save(mut renderer: Option<NonNull<VelloRenderer>>) {
    let renderer = renderer.unwrap_and_deref();

    let last_transform = *renderer.last_transform();

    renderer.transforms.push(last_transform);
    renderer.clips.push(false);
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_restore(mut renderer: Option<NonNull<VelloRenderer>>) {
    let renderer = renderer.unwrap_and_deref();

    renderer.transforms.pop();
    if renderer.clips.pop().unwrap_or_default() {
        renderer.builder.pop_layer();
    }

    if renderer.transforms.is_empty() {
        renderer.transforms.push(Affine::IDENTITY);
        renderer.clips.push(false);
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_transform(
    mut renderer: Option<NonNull<VelloRenderer>>,
    transform: *const [f32; 6],
) {
    let last_transform = renderer.unwrap_and_deref().last_transform();
    *last_transform *= Affine::new((*transform).map(Into::into));
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_draw_path(
    mut renderer: Option<NonNull<VelloRenderer>>,
    mut path: Option<NonNull<VelloPath>>,
    mut paint: Option<NonNull<VelloPaint>>,
) {
    let renderer = renderer.unwrap_and_deref();
    let path = path.unwrap_and_deref();
    let paint = paint.unwrap_and_deref();

    let transform = *renderer.last_transform();

    let builder = &mut renderer.builder;

    let skip_blending = paint.blend_mode == Mix::Normal.into();

    if !skip_blending {
        builder.push_layer(paint.blend_mode, 1.0, transform, &path.path.bounding_box());
    }

    match &paint.style {
        RenderStyle::Fill => builder.fill(path.fill, transform, &paint.brush, None, &path.path),
        RenderStyle::Stroke(stroke) => {
            builder.stroke(stroke, transform, &paint.brush, None, &path.path)
        }
    }

    if !skip_blending {
        builder.pop_layer();
    }
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_clip_path(
    mut renderer: Option<NonNull<VelloRenderer>>,
    mut clip: Option<NonNull<VelloPath>>,
) {
    let renderer = renderer.unwrap_and_deref();

    let transform = *renderer.last_transform();

    if *renderer.last_clip() {
        renderer.builder.pop_layer();
    }

    renderer
        .builder
        .push_layer(Mix::Clip, 1.0, transform, &clip.unwrap_and_deref().path);

    *renderer.last_clip() = true;
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_draw_image(
    mut renderer: Option<NonNull<VelloRenderer>>,
    mut image: Option<NonNull<Image>>,
    blend_mode: rive::BlendMode,
    opacity: f32,
) {
    let renderer = renderer.unwrap_and_deref();
    let image = image.unwrap_and_deref();
    let mix: Mix = blend_mode.into();

    let transform = renderer.last_transform().pre_translate(Vec2::new(
        image.width as f64 * -0.5,
        image.height as f64 * -0.5,
    ));
    let rect = Rect::new(0.0, 0.0, image.width as f64, image.height as f64);

    let builder = &mut renderer.builder;

    let skip_blending = mix == Mix::Normal && opacity == 1.0;

    if skip_blending {
        builder.push_layer(mix, opacity, transform, &rect);
    }

    builder.draw_image(image, transform);

    if skip_blending {
        builder.pop_layer();
    }
}

fn triangle_path(points: [Point; 3]) -> BezPath {
    BezPath::from_path_segments(
        [
            PathSeg::Line(Line::new(points[0], points[1])),
            PathSeg::Line(Line::new(points[1], points[2])),
            PathSeg::Line(Line::new(points[2], points[0])),
        ]
        .into_iter(),
    )
}

#[no_mangle]
pub unsafe extern "C" fn vello_renderer_draw_image_mesh(
    mut renderer: Option<NonNull<VelloRenderer>>,
    mut image: Option<NonNull<Image>>,
    vertices_data: *const rive::Vec2D,
    vertices_len: usize,
    uvs_data: *const rive::Vec2D,
    uvs_len: usize,
    indices_data: *const u16,
    indices_len: usize,
    blend_mode: rive::BlendMode,
    opacity: f32,
) {
    let renderer = renderer.unwrap_and_deref();
    let image = image.unwrap_and_deref();

    let vertices = slice::from_raw_parts(vertices_data, vertices_len);
    let uvs = slice::from_raw_parts(uvs_data, uvs_len);
    let indices = slice::from_raw_parts(indices_data, indices_len);

    let mix: Mix = blend_mode.into();

    for triangle_indices in indices.chunks_exact(3) {
        let points = [
            vertices[triangle_indices[0] as usize],
            vertices[triangle_indices[1] as usize],
            vertices[triangle_indices[2] as usize],
        ];
        let uvs = [
            uvs[triangle_indices[0] as usize],
            uvs[triangle_indices[1] as usize],
            uvs[triangle_indices[2] as usize],
        ];

        let center = Point::new(
            ((points[0].x + points[1].x + points[2].x) / 3.0) as f64,
            ((points[0].y + points[1].y + points[2].y) / 3.0) as f64,
        );

        let path = triangle_path(points.map(|v| Point::new(v.x as f64, v.y as f64)));

        let transform = renderer
            .last_transform()
            .pre_scale_from_origin(1.03, center);
        let brush_transform = util::map_uvs_to_triangle(&points, &uvs, image.width, image.height);

        let builder = &mut renderer.builder;

        let skip_blending = mix == Mix::Normal;

        if !skip_blending {
            builder.push_layer(mix, opacity, transform, &path.bounding_box());
        }

        builder.fill(
            Fill::NonZero,
            transform,
            BrushRef::Image(image),
            Some(brush_transform),
            &path,
        );

        if !skip_blending {
            builder.pop_layer();
        }
    }
}
