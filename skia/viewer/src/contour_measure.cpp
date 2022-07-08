

#include "contour_measure.hpp"
#include "to_skia.hpp"

#include "include/core/SkContourMeasure.h"

namespace rive {

    ContourMeasure::ContourMeasure(const RawPath& path) {
        auto skpath = ToSkia::convert(path);
        m_meas = SkContourMeasureIter(skpath, false).next().release();
    }

    ContourMeasure::~ContourMeasure() { m_meas->unref(); }

    float ContourMeasure::length() const { return m_meas->length(); }

    bool ContourMeasure::computePosTan(float distance, Vec2D* pos, Vec2D* tan) const {
        return m_meas->getPosTan(distance, (SkPoint*)pos, (SkPoint*)tan);
    }

    RawPath ContourMeasure::warp(const RawPath& src) const {
        RawPath dst;

        RawPath::Iter iter(src);
        while (auto rec = iter.next()) {
            switch (rec.verb) {
                case PathVerb::move:
                    dst.move(this->warp(rec.pts[0]));
                    break;
                case PathVerb::line:
                    dst.line(this->warp(rec.pts[0]));
                    break;
                case PathVerb::quad:
                    dst.quad(this->warp(rec.pts[0]), this->warp(rec.pts[1]));
                    break;
                case PathVerb::cubic:
                    dst.cubic(
                        this->warp(rec.pts[0]), this->warp(rec.pts[1]), this->warp(rec.pts[2]));
                    break;
                case PathVerb::close:
                    dst.close();
                    break;
            }
        }
        return dst;
    }

} // namespace rive
