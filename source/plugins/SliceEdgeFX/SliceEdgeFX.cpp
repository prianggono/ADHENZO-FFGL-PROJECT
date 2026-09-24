#include "SliceEdgeFX.h"

using namespace ffglex;

namespace
{
enum
{
    PT_RADIUS_TL = 0,
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
    PT_EFFECT_MIX,
    PT_COUNT
};

static CFFGLPluginInfo PluginInfo(
    PluginFactory<SliceEdgeFX>,
    "AREF",
    "ADHENZO Refine Edge",
    1,
    2,
    1,
    0,
    FF_EFFECT,
    "Rounded slice edge with adjustable stroke and glow.",
    "ADHENZO Creative Technology"
);

const char* VertexShader = R"GLSL(
#version 410 core
uniform vec2 MaxUV;
layout(location = 0) in vec3 vPosition;
layout(location = 2) in vec2 vUV;
out vec2 uv;

void main()
{
    gl_Position = vec4(vPosition, 1.0);
    uv = vUV * MaxUV;
}
)GLSL";

const char* FragmentShader = R"GLSL(
#version 410 core
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
out vec4 FragColor;

float roundedBoxSDF(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float selectedRadius(vec2 p)
{
    if (p.x < 0.0 && p.y < 0.0) return RadiusTL;
    if (p.x > 0.0 && p.y < 0.0) return RadiusTR;
    if (p.x > 0.0 && p.y > 0.0) return RadiusBR;
    return RadiusBL;
}

void main()
{
    vec4 src = texture(InputTexture, uv);

    vec2 p = uv * 2.0 - 1.0;
    float r = clamp(selectedRadius(p), 0.0, 0.5);

    float d = roundedBoxSDF(p, vec2(1.0 - r), r);
    float aa = max(fwidth(d), 0.0001);

    float inside = 1.0 - smoothstep(0.0, aa, d);

    float borderDistance = abs(d);
    float stroke = 1.0 - smoothstep(StrokeWidth, StrokeWidth + aa, borderDistance);
    stroke *= inside;

    float outsideGlow = 1.0 - smoothstep(
        StrokeWidth,
        StrokeWidth + max(GlowSize, aa),
        abs(d)
    );
    outsideGlow *= Glow;
    outsideGlow *= (1.0 - inside);

    vec3 edgeColor = StrokeColor;
    vec3 rgb = mix(src.rgb, edgeColor, stroke * StrokeOpacity);
    rgb = mix(rgb, edgeColor, outsideGlow * StrokeOpacity);

    float alpha = max(src.a * inside, stroke * StrokeOpacity);
    FragColor = vec4(rgb, alpha);
}
)GLSL";
}

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

    mPlugInfo = &PluginInfo;
}

FFResult SliceEdgeFX::InitGL(const FFGLViewportStruct* vp)
{
    shader.Compile(VertexShader, FragmentShader);
    quad.Initialize();
    return CFFGLPlugin::InitGL(vp);
}

FFResult SliceEdgeFX::ProcessOpenGL(ProcessOpenGLStruct* pGL)
{
    if (!pGL || !pGL->numInputTextures || !pGL->inputTextures[0])
        return FF_FAIL;

    FFGLTextureStruct* tex = pGL->inputTextures[0];

    shader.Bind();
    shader.SetSampler("InputTexture", 0);
    shader.SetUniform2f("MaxUV", tex->MaxU, tex->MaxV);
    shader.SetUniform1f("RadiusTL", radiusTL);
    shader.SetUniform1f("RadiusTR", radiusTR);
    shader.SetUniform1f("RadiusBR", radiusBR);
    shader.SetUniform1f("RadiusBL", radiusBL);
    shader.SetUniform1f("StrokeWidth", strokeWidth);
    shader.SetUniform1f("StrokeOpacity", strokeOpacity);
    shader.SetUniform3f("StrokeColor", colorR, colorG, colorB);
    shader.SetUniform1f("Glow", glow);
    shader.SetUniform1f("GlowSize", glowSize);
    shader.SetUniform1f("EffectMix", effectMix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->Handle);
    quad.Draw();
    glBindTexture(GL_TEXTURE_2D, 0);

    shader.Unbind();
    return FF_SUCCESS;
}

FFResult SliceEdgeFX::DeInitGL()
{
    quad.Free();
    shader.Free();
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
