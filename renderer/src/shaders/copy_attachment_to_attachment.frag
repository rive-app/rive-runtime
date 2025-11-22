/*
 * Copyright 2025 Rive
 */

#ifdef @FRAGMENT
layout(input_attachment_index = 0,
       // TODO: This shader is currently only used by MSAA to seed the color
       // buffer, so we do the easy thing and bind to MSAA_COLOR_SEED_IDX.
       // In the future we may need to set up new bindings for this input
       // attachment.
       binding = MSAA_COLOR_SEED_IDX,
       set = PLS_TEXTURE_BINDINGS_SET) uniform lowp subpassInput
    inputAttachment;

layout(location = 0) out half4 outputColor;

void main() { outputColor = subpassLoad(inputAttachment); }
#endif
