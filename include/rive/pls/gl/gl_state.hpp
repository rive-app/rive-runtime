/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include "rive/refcnt.hpp"

namespace rive::pls
{
// Lightweight wrapper around common GL state.
class GLState : public RefCnt<GLState>
{
public:
    GLState(const GLCapabilities& extensions) { reset(extensions); }

    void reset(const GLCapabilities&);

    void enableFaceCulling(bool);

    void bindProgram(GLuint);
    void bindVAO(GLuint);
    void bindBuffer(GLenum target, GLuint);

    void deleteProgram(GLuint);
    void deleteVAO(GLuint);
    void deleteBuffer(GLuint);

private:
    bool m_faceCullingEnabled;
    GLuint m_boundProgramID;
    GLuint m_boundVAO;
    GLuint m_boundArrayBufferID;
    GLuint m_boundUniformBufferID;
    GLuint m_boundPixelUnpackBufferID;

    struct
    {
        bool faceCullingEnabled : 1;
        bool boundProgramID : 1;
        bool boundVAO : 1;
        bool boundArrayBufferID : 1;
        bool boundUniformBufferID : 1;
        bool boundPixelUnpackBufferID : 1;
    } m_validState;
};
} // namespace rive::pls
