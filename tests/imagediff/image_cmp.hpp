#ifndef _RIVE_IMAGECMP_HPP_
#define _RIVE_IMAGECMP_HPP_

#include "SkImage.h"
#include <cstdint>

struct ImageCmp
{
    bool images_loaded;
    bool dimensions_equal;
    int max_component_diff;   // 0..255
    float ave_component_diff; // 0..1
    size_t pixel_diffcount;

    bool identical() const
    {
        return images_loaded && dimensions_equal && max_component_diff == 0 &&
               ave_component_diff == 0 && pixel_diffcount == 0;
    }

    static ImageCmp failedToLoad() { return {false, false, 0, 0}; }
    static ImageCmp dimensionMismatch() { return {true, false, 0, 0}; }
    static ImageCmp diff(int max, float ave, size_t pixel_diffcount)
    {
        return {true, true, max, ave, pixel_diffcount};
    }
};

ImageCmp image_cmp(sk_sp<SkImage> a, sk_sp<SkImage> b);

uint32_t compute_diff0(uint32_t, uint32_t);
uint32_t compute_diff1(uint32_t, uint32_t);

sk_sp<SkImage> make_diff_image(sk_sp<SkImage> a,
                               sk_sp<SkImage> b,
                               uint32_t (*compute_diff)(uint32_t a, uint32_t b));

/* Always appends "name " + some info to the statusPath file
 *
 * if fails to read the images
 *      statusFile += "failed"
 *  elif pathA and pathB are identical
 *      statusFile += "identical"
 *  else if the images have different dimensions
 *      statusFile += "sizemismatch"
 *  else
 *      statusFile += "max_component_diff ave_compoent_diff"
 *      pathDir    += name_diff.png and name_maxdiff.png
 */
void image_diff(const char name[],
                const char pathA[],
                const char pathB[],
                const char statusPath[],
                const char diffDir[]);
#endif
