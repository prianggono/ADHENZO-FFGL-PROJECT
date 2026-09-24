#include "SliceEdgeFX.h"
using namespace ffglex;

enum ParamType : FFUInt32
{
    PT_ROUNDNESS
};

static CFFGLPluginInfo PluginInfo(
    PluginFactory<SliceEdgeFX>,
    "AREF",
    "ADHENZO Refine Edge",
    2, 1,
    1, 2,
    FF_EFFECT,
    "Illustrator-style rounded rectangle mask for Resolume slices.",
    "ADHENZO Creative Technology"
);

static const char VertexShader[] = R"(#version 410 core
uniform vec2 MaxUV;
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
out vec2 uv;

void main()
{
    gl_Position = vPosition;
    uv = vUV * MaxUV;
}
)";

static const char FragmentShader[] = R"(#version 410 core
uniform sampler2D InputTexture;
uniform float Roundness;
uniform vec2 Aspect;

in vec2 uv;
out vec4 fragColor;

// Signed distance to a rectangle with a true circular corner radius.
// The coordinate system is aspect-correct, so a 16:9 rectangle gets
// round corners like Illustrator instead of stretched elliptical corners.
float roundedRectSDF(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0))
         + min(max(q.x, q.y), 0.0)
         - radius;
}

void main()
{
    vec4 src = texture(InputTexture, uv);

    // Center the input and correct the X axis for the actual texture aspect.
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= Aspect.x;

    vec2 halfSize = vec2(Aspect.x, 1.0);

    // 0..1 maps to 0..100% of the short half-dimension.
    // At 100%, a landscape rectangle becomes a capsule.
    float maxRadius = min(halfSize.x, halfSize.y);
    float radius = clamp(Roundness, 0.0, 1.0) * maxRadius;

    float d = roundedRectSDF(p, halfSize, radius);

    // One-pixel-ish antialiasing in normalized coordinates.
    float aa = max(fwidth(d), 0.00001);
    float mask = 1.0 - smoothstep(0.0, aa, d);

    // Preserve the original image inside the rounded path and make
    // everything outside the path transparent.
    fragColor = vec4(src.rgb, src.a * mask);
}
)";

SliceEdgeFX::SliceEdgeFX()
    : roundness(0.0f)
{
    SetMinInputs(1);
    SetMaxInputs(1);

    SetParamInfof(PT_ROUNDNESS, "Roundness", FF_TYPE_STANDARD);

    FFGLLog::LogToHost("Created ADHENZO Refine Edge V1");
}

FFResult SliceEdgeFX::InitGL(const FFGLViewportStruct* vp)
{
    if (!shader.Compile(VertexShader, FragmentShader))
    {
        DeInitGL();
        return FF_FAIL;
    }

    if (!quad.Initialise())
    {
        DeInitGL();
        return FF_FAIL;
    }

    return CFFGLPlugin::InitGL(vp);
}

FFResult SliceEdgeFX::ProcessOpenGL(ProcessOpenGLStruct* pGL)
{
    if (pGL == nullptr || pGL->numInputTextures < 1 || pGL->inputTextures[0] == nullptr)
        return FF_FAIL;

    ScopedShaderBinding shaderBinding(shader.GetGLID());
    ScopedSamplerActivation activateSampler(0);
    Scoped2DTextureBinding textureBinding(pGL->inputTextures[0]->Handle);

    shader.Set("InputTexture", 0);

    const FFGLTextureStruct& texture = *pGL->inputTextures[0];

    FFGLTexCoords maxCoords = GetMaxGLTexCoords(texture);
    shader.Set("MaxUV", maxCoords.s, maxCoords.t);

    float width = static_cast<float>(texture.Width);
    float height = static_cast<float>(texture.Height);
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;

    glUniform1f(shader.FindUniform("Roundness"), roundness);
    glUniform2f(shader.FindUniform("Aspect"), aspect, 1.0f);

    quad.Draw();

    return FF_SUCCESS;
}

FFResult SliceEdgeFX::DeInitGL()
{
    shader.FreeGLResources();
    quad.Release();
    return FF_SUCCESS;
}

FFResult SliceEdgeFX::SetFloatParameter(unsigned int index, float value)
{
    switch (index)
    {
    case PT_ROUNDNESS:
        roundness = value;
        return FF_SUCCESS;

    default:
        return FF_FAIL;
    }
}

float SliceEdgeFX::GetFloatParameter(unsigned int index)
{
    switch (index)
    {
    case PT_ROUNDNESS:
        return roundness;

    default:
        return 0.0f;
    }
}
