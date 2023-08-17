use vello::peniko::{Mix, Stroke};

use crate::RenderStyle;

#[repr(C)]
#[derive(Debug, Default)]
pub enum RenderPaint {
    Stroke,
    #[default]
    Fill,
}

impl From<RenderPaint> for RenderStyle {
    fn from(value: RenderPaint) -> Self {
        match value {
            RenderPaint::Stroke => Self::Stroke(Stroke::new(0.0)),
            RenderPaint::Fill => Self::Fill,
        }
    }
}

#[repr(u32)]
#[derive(Debug, Default)]
pub enum StrokeJoin {
    #[default]
    Miter = 0,
    Round = 1,
    Bevel = 2,
}

#[repr(u32)]
#[derive(Debug, Default)]
pub enum StrokeCap {
    #[default]
    Butt = 0,
    Round = 1,
    Square = 2,
}

#[repr(u32)]
#[derive(Debug, Default)]
pub enum BlendMode {
    #[default]
    SrcOver = 3,
    Screen = 14,
    Overlay = 15,
    Darken = 16,
    Lighten = 17,
    ColorDodge = 18,
    ColorBurn = 19,
    HardLight = 20,
    SoftLight = 21,
    Difference = 22,
    Exclusion = 23,
    Multiply = 24,
    Hue = 25,
    Saturation = 26,
    Color = 27,
    Luminosity = 28,
}

impl From<BlendMode> for Mix {
    fn from(value: BlendMode) -> Self {
        match value {
            BlendMode::SrcOver => Self::Normal,
            BlendMode::Screen => Self::Screen,
            BlendMode::Overlay => Self::Overlay,
            BlendMode::Darken => Self::Darken,
            BlendMode::Lighten => Self::Lighten,
            BlendMode::ColorDodge => Self::ColorDodge,
            BlendMode::ColorBurn => Self::ColorBurn,
            BlendMode::HardLight => Self::HardLight,
            BlendMode::SoftLight => Self::SoftLight,
            BlendMode::Difference => Self::Difference,
            BlendMode::Exclusion => Self::Exclusion,
            BlendMode::Multiply => Self::Multiply,
            BlendMode::Hue => Self::Hue,
            BlendMode::Saturation => Self::Saturation,
            BlendMode::Color => Self::Color,
            BlendMode::Luminosity => Self::Luminosity,
        }
    }
}

#[repr(C)]
#[derive(Debug, Default)]
pub enum FillRule {
    #[default]
    NonZero,
    EvenOdd,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Vec2D {
    pub x: f32,
    pub y: f32,
}
