use std::{
    ops::{Add, Mul, Neg, Sub},
    ptr::NonNull,
};

use vello::kurbo::{Affine, Point};

use crate::rive;

pub trait UnwrapAndDeref {
    type Deref;

    unsafe fn unwrap_and_deref(&mut self) -> &mut Self::Deref;
}

impl<T> UnwrapAndDeref for Option<NonNull<T>> {
    type Deref = T;

    unsafe fn unwrap_and_deref(&mut self) -> &mut Self::Deref {
        self.map(|mut ptr| ptr.as_mut()).unwrap()
    }
}

#[derive(Clone, Copy, Debug)]
struct Vec2 {
    x: f32,
    y: f32,
}

impl Vec2 {
    pub fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }

    pub fn splat(val: f32) -> Self {
        Self::new(val, val)
    }
}

impl Add for Vec2 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self::new(self.x + rhs.x, self.y + rhs.y)
    }
}

impl Sub for Vec2 {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self::new(self.x - rhs.x, self.y - rhs.y)
    }
}

impl Mul for Vec2 {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self::Output {
        Self::new(self.x * rhs.x, self.y * rhs.y)
    }
}

impl Mul<f32> for Vec2 {
    type Output = Self;

    fn mul(self, rhs: f32) -> Self::Output {
        self * Self::splat(rhs)
    }
}

impl Neg for Vec2 {
    type Output = Self;

    fn neg(self) -> Self::Output {
        Self::new(-self.x, -self.y)
    }
}

/// Finds the affine transform that maps triangle `from` to triangle `to`. The algorithm is based
/// on the [Simplex Affine Mapping] method which has a [Swift implementation].
///
/// [Simplex Affine Mapping]: https://www.researchgate.net/publication/332410209_Beginner%27s_guide_to_mapping_simplexes_affinely
/// [Swift implementation]: https://rethunk.medium.com/finding-an-affine-transform-using-three-2d-point-correspondences-using-simplex-affine-mapping-255aeb4e8055
fn simplex_affine_mapping(from: [Vec2; 3], to: [Vec2; 3]) -> Affine {
    let [a, b, c] = from;
    let [d, e, f] = to;

    let det_recip = (a.x * b.y + b.x * c.y + c.x * a.y - a.x * c.y - b.x * a.y - c.x * b.y).recip();

    let p = (d * (b.y - c.y) - e * (a.y - c.y) + f * (a.y - b.y)) * det_recip;

    let q = (e * (a.x - c.x) - d * (b.x - c.x) - f * (a.x - b.x)) * det_recip;

    let t = (d * (b.x * c.y - b.y * c.x) - e * (a.x * c.y - a.y * c.x)
        + f * (a.x * b.y - a.y * b.x))
        * det_recip;

    Affine::new([
        p.x as f64, p.y as f64, q.x as f64, q.y as f64, t.x as f64, t.y as f64,
    ])
}

pub fn map_uvs_to_triangle(
    points: &[rive::Vec2D; 3],
    uvs: &[rive::Vec2D; 3],
    width: u32,
    height: u32,
) -> Affine {
    simplex_affine_mapping(
        uvs.map(|v| Vec2::new(v.x * width as f32, v.y * height as f32)),
        points.map(|v| Vec2::new(v.x, v.y)),
    )
}

pub trait ScaleFromOrigin {
    fn pre_scale_from_origin(self, scale: f64, origin: Point) -> Self;
}

impl ScaleFromOrigin for Affine {
    fn pre_scale_from_origin(self, scale: f64, origin: Point) -> Self {
        let origin = origin.to_vec2();
        self.pre_translate(origin)
            .pre_scale(scale)
            .pre_translate(-origin)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use vello::kurbo;

    #[test]
    fn straightforward_sam() {
        let from = [
            Vec2::new(1.0, 1.0),
            Vec2::new(3.0, 1.0),
            Vec2::new(1.0, 3.0),
        ];
        let to = [
            Vec2::new(4.0, 4.0),
            Vec2::new(4.0, -1.0),
            Vec2::new(-2.0, 4.0),
        ];

        let mapping = simplex_affine_mapping(from, to);

        for (from, to) in from.into_iter().zip(to.into_iter()) {
            let res = mapping * kurbo::Point::new(from.x as f64, from.y as f64);

            assert_eq!(res.x as f32, to.x);
            assert_eq!(res.y as f32, to.y);
        }
    }
}
