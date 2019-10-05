#pragma once

#include "servers/visual_server_enums.h"
#include "core/method_arg_casters.h"
#include "core/method_enum_caster.h"

// make variant understand the enums
VARIANT_ENUM_CAST(VS::CubeMapSide);
VARIANT_ENUM_CAST(VS::TextureFlags);
VARIANT_ENUM_CAST(VS::ShaderMode);
VARIANT_ENUM_CAST(VS::ArrayType);
VARIANT_ENUM_CAST(VS::ArrayFormat);
VARIANT_ENUM_CAST(VS::PrimitiveType);
VARIANT_ENUM_CAST(VS::BlendShapeMode);
VARIANT_ENUM_CAST(VS::LightType);
VARIANT_ENUM_CAST(VS::LightParam);
VARIANT_ENUM_CAST(VS::ViewportUpdateMode);
VARIANT_ENUM_CAST(VS::ViewportClearMode);
VARIANT_ENUM_CAST(VS::ViewportMSAA);
VARIANT_ENUM_CAST(VS::ViewportUsage);
VARIANT_ENUM_CAST(VS::ViewportRenderInfo);
VARIANT_ENUM_CAST(VS::ViewportDebugDraw);
VARIANT_ENUM_CAST(VS::ScenarioDebugMode);
VARIANT_ENUM_CAST(VS::InstanceType);
VARIANT_ENUM_CAST(VS::NinePatchAxisMode);
VARIANT_ENUM_CAST(VS::CanvasLightMode);
VARIANT_ENUM_CAST(VS::CanvasLightShadowFilter);
VARIANT_ENUM_CAST(VS::CanvasOccluderPolygonCullMode);
VARIANT_ENUM_CAST(VS::RenderInfo);
VARIANT_ENUM_CAST(VS::Features);
VARIANT_ENUM_CAST(VS::MultimeshTransformFormat);
VARIANT_ENUM_CAST(VS::MultimeshColorFormat);
VARIANT_ENUM_CAST(VS::MultimeshCustomDataFormat);
VARIANT_ENUM_CAST(VS::LightOmniShadowMode);
VARIANT_ENUM_CAST(VS::LightOmniShadowDetail);
VARIANT_ENUM_CAST(VS::LightDirectionalShadowMode);
VARIANT_ENUM_CAST(VS::LightDirectionalShadowDepthRangeMode);
VARIANT_ENUM_CAST(VS::ReflectionProbeUpdateMode);
VARIANT_ENUM_CAST(VS::ParticlesDrawOrder);
VARIANT_ENUM_CAST(VS::EnvironmentBG);
VARIANT_ENUM_CAST(VS::EnvironmentDOFBlurQuality);
VARIANT_ENUM_CAST(VS::EnvironmentGlowBlendMode);
VARIANT_ENUM_CAST(VS::EnvironmentToneMapper);
VARIANT_ENUM_CAST(VS::EnvironmentSSAOQuality);
VARIANT_ENUM_CAST(VS::EnvironmentSSAOBlur);
VARIANT_ENUM_CAST(VS::InstanceFlags);
VARIANT_ENUM_CAST(VS::ShadowCastingSetting);
VARIANT_ENUM_CAST(VS::TextureType);
