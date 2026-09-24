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
    bool equalCorners;

    float radiusTL;
    float radiusTR;
    float radiusBR;
    float radiusBL;

    float softEdge;

    float strokeWidth;
    float strokeOpacity;

    float colorR;
    float colorG;
    float colorB;

    float glow;
    float glowSize;

    float effectMix;
};
