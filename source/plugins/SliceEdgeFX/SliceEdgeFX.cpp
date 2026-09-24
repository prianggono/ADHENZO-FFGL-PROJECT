#include "SliceEdgeFX.h"
using namespace ffglex;

enum ParamType : FFUInt32
{
    PT_ROUNDNESS,
    PT_EQUAL_CORNERS,
    PT_RADIUS_TL,
    PT_RADIUS_TR,
    PT_RADIUS_BR,
    PT_RADIUS_BL,
    PT_SOFT_EDGE,
    PT_STROKE_WIDTH,
    PT_STROKE_OPACITY,
    PT_COLOR_R,
    PT_COLOR_G,
    PT_COLOR_B,
    PT_GLOW,
    PT_GLOW_SIZE,
    PT_EFFECT_MIX
};

static CFFGLPluginInfo PluginInfo(
    PluginFactory<SliceEdgeFX>,
    "AREF",
    "ADHENZO Refine Edge",
    2, 1,
    1, 1,
    FF_EFFECT,
    "Refiner-style rounded slice mask with soft edges, stroke and glow.",
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
uniform float EqualCorners;

uniform float RadiusTL;
uniform float RadiusTR;
uniform float RadiusBR;
uniform float RadiusBL;

uniform float SoftEdge;
uniform float StrokeWidth;
uniform float StrokeOpacity;
uniform vec3 StrokeColor;

uniform float Glow;
uniform float GlowSize;
uniform float EffectMix;

in vec2 uv;
out vec4 fragColor;

float roundedBoxSDF(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float getCornerRadius(vec2 p)
{
    float r = Roundness;

    if (EqualCorners > 0.5)
        return r;

    if (p.x < 0.0 && p.y < 0.0) return max(r, RadiusTL);
    if (p.x >= 0.0 && p.y < 0.0) return max(r, RadiusTR);
    if (p.x >= 0.0 && p.y >= 0.0) return max(r, RadiusBR);
    return max(r, RadiusBL);
}

void main()
{
    vec4 src = texture(InputTexture, uv);

    vec2 p = uv * 2.0 - 1.0;

    // FFGL controls are normalized. Keep the radius below half the shape size.
    float r = clamp(getCornerRadius(p), 0.0, 0.95);

    float d = roundedBoxSDF(p, vec2(1.0 - r), r);
    float aa = max(fwidth(d), 0.0005);

    // Soft Edge controls the alpha falloff outside the shape.
    float soft = max(SoftEdge, 0.0);
    float edgeWidth = max(aa, soft * 0.08);

    float mask = 1.0 - smoothstep(0.0, edgeWidth, d);

    // Stroke follows the actual shape boundary.
    float strokeWidth = max(StrokeWidth, 0.0);
    float stroke = 1.0 - smoothstep(
        strokeWidth,
        strokeWidth + aa,
        abs(d)
    );
    stroke *= StrokeOpacity;

    // Glow extends outside the mask.
    float glowWidth = max(GlowSize, aa);
    float glowBand = 1.0 - smoothstep(
        0.0,
        glowWidth,
        max(d, 0.0)
    );
    glowBand *= Glow * StrokeOpacity;

    float edgeAmount = clamp(stroke + glowBand, 0.0, 1.0);

    // Keep the source image inside the rounded mask.
    // Outside the mask only the edge/glow is visible.
    float alpha = src.a * max(mask, edgeAmount);

    vec3 sourceRgb = src.a > 0.00001
        ? src.rgb / src.a
        : vec3(0.0);

    vec3 rgb = mix(sourceRgb, StrokeColor, edgeAmount);

    // Premultiplied output expected by the FFGL pipeline.
    vec4 effected = vec4(rgb * alpha, alpha);

    fragColor = mix(src, effected, clamp(EffectMix, 0.0, 1.0));
}
)";

SliceEdgeFX::SliceEdgeFX()
    : roundness(0.08f)
    , equalCorners(1.0f)
    , radiusTL(0.08f)
    , radiusTR(0.08f)
    , radiusBR(0.08f)
    , radiusBL(0.08f)
    , softEdge(0.0f)
    , strokeWidth(0.012f)
    , strokeOpacity(1.0f)
    , colorR(1.0f)
    , colorG(0.55f)
    , colorB(0.0f)
    , glow(0.0f)
    , glowSize(0.03f)
    , effectMix(1.0f)
{
    SetMinInputs(1);
    SetMaxInputs(1);

    SetParamInfof(PT_ROUNDNESS, "Roundness", FF_TYPE_STANDARD);
    SetParamInfo(PT_EQUAL_CORNERS, "Equal Corners", FF_TYPE_BOOLEAN, equalCorners);

    SetParamInfof(PT_RADIUS_TL, "Top Left", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_TR, "Top Right", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_BR, "Bottom Right", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_BL, "Bottom Left", FF_TYPE_STANDARD);

    SetParamInfof(PT_SOFT_EDGE, "Soft Edge", FF_TYPE_STANDARD);
    SetParamInfof(PT_STROKE_WIDTH, "Stroke Width", FF_TYPE_STANDARD);
    SetParamInfof(PT_STROKE_OPACITY, "Stroke Opacity", FF_TYPE_STANDARD);

    SetParamInfof(PT_COLOR_R, "Stroke Red", FF_TYPE_RED);
    SetParamInfof(PT_COLOR_G, "Stroke Green", FF_TYPE_GREEN);
    SetParamInfof(PT_COLOR_B, "Stroke Blue", FF_TYPE_BLUE);

    SetParamInfof(PT_GLOW, "Glow", FF_TYPE_STANDARD);
    SetParamInfof(PT_GLOW_SIZE, "Glow Size", FF_TYPE_STANDARD);
    SetParamInfof(PT_EFFECT_MIX, "Effect Mix", FF_TYPE_STANDARD);

    FFGLLog::LogToHost("Created ADHENZO Refine Edge");
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

    FFGLTexCoords maxCoords = GetMaxGLTexCoords(*pGL->inputTextures[0]);
    shader.Set("MaxUV", maxCoords.s, maxCoords.t);

    glUniform1f(shader.FindUniform("Roundness"), roundness);
    glUniform1f(shader.FindUniform("EqualCorners"), equalCorners ? 1.0f : 0.0f);

    glUniform1f(shader.FindUniform("RadiusTL"), radiusTL);
    glUniform1f(shader.FindUniform("RadiusTR"), radiusTR);
    glUniform1f(shader.FindUniform("RadiusBR"), radiusBR);
    glUniform1f(shader.FindUniform("RadiusBL"), radiusBL);

    glUniform1f(shader.FindUniform("SoftEdge"), softEdge);
    glUniform1f(shader.FindUniform("StrokeWidth"), strokeWidth);
    glUniform1f(shader.FindUniform("StrokeOpacity"), strokeOpacity);
    glUniform3f(shader.FindUniform("StrokeColor"), colorR, colorG, colorB);

    glUniform1f(shader.FindUniform("Glow"), glow);
    glUniform1f(shader.FindUniform("GlowSize"), glowSize);
    glUniform1f(shader.FindUniform("EffectMix"), effectMix);

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
    case PT_ROUNDNESS: roundness = value; break;
    case PT_EQUAL_CORNERS: equalCorners = value >= 0.5f; break;

    case PT_RADIUS_TL: radiusTL = value; break;
    case PT_RADIUS_TR: radiusTR = value; break;
    case PT_RADIUS_BR: radiusBR = value; break;
    case PT_RADIUS_BL: radiusBL = value; break;

    case PT_SOFT_EDGE: softEdge = value; break;
    case PT_STROKE_WIDTH: strokeWidth = value; break;
    case PT_STROKE_OPACITY: strokeOpacity = value; break;

    case PT_COLOR_R: colorR = value; break;
    case PT_COLOR_G: colorG = value; break;
    case PT_COLOR_B: colorB = value; break;

    case PT_GLOW: glow = value; break;
    case PT_GLOW_SIZE: glowSize = value; break;
    case PT_EFFECT_MIX: effectMix = value; break;

    default:
        return FF_FAIL;
    }

    return FF_SUCCESS;
}

float SliceEdgeFX::GetFloatParameter(unsigned int index)
{
    switch (index)
    {
    case PT_ROUNDNESS: return roundness;
    case PT_EQUAL_CORNERS: return equalCorners ? 1.0f : 0.0f;

    case PT_RADIUS_TL: return radiusTL;
    case PT_RADIUS_TR: return radiusTR;
    case PT_RADIUS_BR: return radiusBR;
    case PT_RADIUS_BL: return radiusBL;

    case PT_SOFT_EDGE: return softEdge;
    case PT_STROKE_WIDTH: return strokeWidth;
    case PT_STROKE_OPACITY: return strokeOpacity;

    case PT_COLOR_R: return colorR;
    case PT_COLOR_G: return colorG;
    case PT_COLOR_B: return colorB;

    case PT_GLOW: return glow;
    case PT_GLOW_SIZE: return glowSize;
    case PT_EFFECT_MIX: return effectMix;

    default:
        return 0.0f;
    }
}
