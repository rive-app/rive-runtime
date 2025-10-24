/*
 * Copyright 2025 Rive
 */

#ifdef @FRAGMENT

PLS_BLOCK_BEGIN
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
#endif
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageBuffer);
PLS_BLOCK_END

PLS_MAIN(@drawFragmentMain)
{
    VARYING_UNPACK(v_clipIDs, half2);
    half clipID = -v_clipIDs.x;

#ifdef @DRAW_INTERIOR_TRIANGLES
    VARYING_INIT(v_windingWeight, half);
    half fragCoverage = v_windingWeight;
#else
    VARYING_INIT(v_coverages, COVERAGE_TYPE);
    half fragCoverage = v_coverages.x;
#endif //@DRAW_INTERIOR_TRIANGLES

    PLS_INTERLOCK_BEGIN;

#if defined(@DRAW_INTERIOR_TRIANGLES) && defined(@BORROWED_COVERAGE_PASS)
    // Interior triangles with borrowed coverage are always the first fragment
    // of the path at their pixel, so we don't need to check the current
    // coverage value.
    half clipCoverage = fragCoverage;
#else
    half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
    half clipBufferID = clipData.g;
    half initialCoverage = clipBufferID == clipID ? clipData.r : make_half(.0);
    half clipCoverage = initialCoverage + fragCoverage;
#endif

#ifdef @ENABLE_NESTED_CLIPPING
    half outerClipID = v_clipIDs.y;
    if (@ENABLE_NESTED_CLIPPING && outerClipID != .0)
    {
        half outerClipCoverage = .0;
#if defined(@DRAW_INTERIOR_TRIANGLES) && defined(@BORROWED_COVERAGE_PASS)
        // Interior triangles with borrowed coverage did not load the clip
        // buffer already, so do that now.
        half2 clipData = unpackHalf2x16(PLS_LOADUI(clipBuffer));
        half clipBufferID = clipData.g;
#endif
        if (clipBufferID != clipID)
        {
            outerClipCoverage = clipBufferID == outerClipID ? clipData.r : .0;
            // The coverage buffer is a free resource when rendering clip
            // because we don't use it to track coverage. Use it instead to
            // temporarily save the outer clip for nested clips. But make sure
            // to write an invalid pathID so we don't corrupt any future paths
            // that will be drawn.
            PLS_STOREUI(
                coverageBuffer,
                packHalf2x16(make_half2(outerClipCoverage, INVALID_PATH_ID)));
        }
        else
        {
            outerClipCoverage = unpackHalf2x16(PLS_LOADUI(coverageBuffer)).r;
            PLS_PRESERVE_UI(coverageBuffer);
        }
        clipCoverage = min(clipCoverage, outerClipCoverage);
    }
    else
#endif
    {
        PLS_PRESERVE_UI(coverageBuffer);
    }

    PLS_STOREUI(clipBuffer, packHalf2x16(make_half2(clipCoverage, clipID)));
#ifndef @FIXED_FUNCTION_COLOR_OUTPUT
    PLS_PRESERVE_4F(colorBuffer);
#endif
    PLS_INTERLOCK_END;

    EMIT_PLS;
}

#endif // FRAGMENT
