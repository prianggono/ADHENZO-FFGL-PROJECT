#include "SliceEdgeFX.h"
using namespace ffglex;

enum ParamType : FFUInt32
{
    PT_RADIUS_TL,
    PT_RADIUS_TR,
    PT_RADIUS_BR,
    PT_RADIUS_BL,
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
    1, 0,
    FF_EFFECT,
    "Refine slice edges with independent corner radius, stroke and glow.",
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
uniform float RadiusTL;
uniform float RadiusTR;
uniform float RadiusBR;
uniform float RadiusBL;
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

float cornerRadius(vec2 p)
{
    if (p.x < 0.0 && p.y < 0.0) return RadiusTL;
    if (p.x >= 0.0 && p.y < 0.0) return RadiusTR;
    if (p.x >= 0.0 && p.y >= 0.0) return RadiusBR;
    return RadiusBL;
}

void main()
{
    vec4 src = texture(InputTexture, uv);

    vec2 p = uv * 2.0 - 1.0;
    float r = clamp(cornerRadius(p), 0.0, 1.0);

    // Keep the radius inside the half-size of the normalized rectangle.
    r = min(r, 0.999);

    float d = roundedBoxSDF(p, vec2(1.0 - r), r);
    float aa = max(fwidth(d), 0.0001);

    float inside = 1.0 - smoothstep(0.0, aa, d);

    float stroke = 1.0 - smoothstep(
        StrokeWidth,
        StrokeWidth + aa,
        abs(d)
    );
    stroke *= inside * StrokeOpacity;

    float glowBand = 1.0 - smoothstep(
        StrokeWidth,
        StrokeWidth + max(GlowSize, aa),
        abs(d)
    );
    glowBand *= (1.0 - inside) * Glow * StrokeOpacity;

    vec3 straightRgb = src.a > 0.00001 ? src.rgb / src.a : vec3(0.0);
    vec3 effected = mix(straightRgb, StrokeColor, clamp(stroke + glowBand, 0.0, 1.0));
    float alpha = src.a * inside;

    // FFGL expects premultiplied color.
    vec3 outRgb = effected * alpha;

    fragColor = mix(src, vec4(outRgb, alpha), clamp(EffectMix, 0.0, 1.0));
}
)";

SliceEdgeFX::SliceEdgeFX()
    : radiusTL(0.08f)
    , radiusTR(0.08f)
    , radiusBR(0.08f)
    , radiusBL(0.08f)
    , strokeWidth(0.012f)
    , strokeOpacity(1.0f)
    , colorR(1.0f)
    , colorG(1.0f)
    , colorB(1.0f)
    , glow(0.0f)
    , glowSize(0.03f)
    , effectMix(1.0f)
{
    SetMinInputs(1);
    SetMaxInputs(1);

    SetParamInfof(PT_RADIUS_TL, "Radius TL", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_TR, "Radius TR", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_BR, "Radius BR", FF_TYPE_STANDARD);
    SetParamInfof(PT_RADIUS_BL, "Radius BL", FF_TYPE_STANDARD);
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

    glUniform1f(shader.FindUniform("RadiusTL"), radiusTL);
    glUniform1f(shader.FindUniform("RadiusTR"), radiusTR);
    glUniform1f(shader.FindUniform("RadiusBR"), radiusBR);
    glUniform1f(shader.FindUniform("RadiusBL"), radiusBL);
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
    case PT_RADIUS_TL: radiusTL = value; break;
    case PT_RADIUS_TR: radiusTR = value; break;
    case PT_RADIUS_BR: radiusBR = value; break;
    case PT_RADIUS_BL: radiusBL = value; break;
    case PT_STROKE_WIDTH: strokeWidth = value; break;
    case PT_STROKE_OPACITY: strokeOpacity = value; break;
    case PT_COLOR_R: colorR = value; break;
    case PT_COLOR_G: colorG = value; break;
    case PT_COLOR_B: colorB = value; break;
    case PT_GLOW: glow = value; break;
    case PT_GLOW_SIZE: glowSize = value; break;
    case PT_EFFECT_MIX: effectMix = value; break;
    default: return FF_FAIL;
    }
    return FF_SUCCESS;
}

float SliceEdgeFX::GetFloatParameter(unsigned int index)
{
    switch (index)
    {
    case PT_RADIUS_TL: return radiusTL;
    case PT_RADIUS_TR: return radiusTR;
    case PT_RADIUS_BR: return radiusBR;
    case PT_RADIUS_BL: return radiusBL;
    case PT_STROKE_WIDTH: return strokeWidth;
    case PT_STROKE_OPACITY: return strokeOpacity;
    case PT_COLOR_R: return colorR;
    case PT_COLOR_G: return colorG;
    case PT_COLOR_B: return colorB;
    case PT_GLOW: return glow;
    case PT_GLOW_SIZE: return glowSize;
    case PT_EFFECT_MIX: return effectMix;
    default: return 0.0f;
    }
}
