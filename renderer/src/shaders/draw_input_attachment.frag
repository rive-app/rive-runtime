/*
 * Copyright 2025 Rive
 */

#ifdef @FRAGMENT
layout(input_attachment_index = 0,
#ifdef @INPUT_ATTACHMENT_BINDING
       binding = @INPUT_ATTACHMENT_BINDING,
#else
       binding = 0,
#endif
       set = PLS_TEXTURE_BINDINGS_SET) uniform lowp subpassInput
    inputAttachment;

layout(location = 0) out half4 outputColor;

void main() { outputColor = subpassLoad(inputAttachment); }
#endif
