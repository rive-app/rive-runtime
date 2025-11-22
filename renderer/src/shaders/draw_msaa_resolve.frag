/*
 * Copyright 2025 Rive
 */

#ifdef @FRAGMENT
layout(input_attachment_index = 0,
       binding = COLOR_PLANE_IDX,
       set = PLS_TEXTURE_BINDINGS_SET) uniform lowp subpassInputMS msaaColor;

layout(location = 0) out half4 outputColor;

void main()
{
    outputColor = (subpassLoad(msaaColor, 0) + subpassLoad(msaaColor, 1) +
                   subpassLoad(msaaColor, 2) + subpassLoad(msaaColor, 3)) *
                  .25;
}
#endif
