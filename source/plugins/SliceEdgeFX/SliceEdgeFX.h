#pragma once

#include <FFGLSDK.h>

class SliceEdgeFX : public CFFGLPlugin
{
public:
    SliceEdgeFX();
    ~SliceEdgeFX() override = default;

    FFResult InitGL(const FFGLViewportStruct* vp) override;
    FFResult ProcessOpenGL(ProcessOpenGLStruct* pGL) override;
    FFResult DeInitGL() override;

    FFResult SetFloatParameter(unsigned int index, float value) override;
    float GetFloatParameter(unsigned int index) override;

private:
    ffglex::FFGLShader shader;
    ffglex::FFGLScreenQuad quad;

    float roundness;
};
