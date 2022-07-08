

#ifndef _RIVE_CONTOUR_MEASURE_HPP_
#define _RIVE_CONTOUR_MEASURE_HPP_

#include "rive/math/raw_path.hpp"

class SkContourMeasure;

namespace rive {

    class ContourMeasure {
        SkContourMeasure* m_meas;

    public:
        ContourMeasure(const RawPath& path);
        ~ContourMeasure();

        float length() const;

        bool computePosTan(float distance, Vec2D* pos, Vec2D* tan) const;

        bool warp(Vec2D src, Vec2D* dst) const {
            Vec2D pos, tan;
            if (this->computePosTan(src.x, &pos, &tan)) {
                *dst = {
                    pos.x - tan.y * src.y,
                    pos.y + tan.x * src.y,
                };
                return true;
            }
            return false;
        }

        Vec2D warp(Vec2D point) const {
            Vec2D result;
            return this->warp(point, &result) ? result : Vec2D{0, 0};
        }

        RawPath warp(const RawPath&) const;
    };
} // namespace rive

#endif
