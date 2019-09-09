#pragma once

#include "servers/visual_server.h"
#include "core/method_enum_caster.h"

// make variant understand the enums
VARIANT_ENUM_CAST(VisualServer::CubeMapSide);
VARIANT_ENUM_CAST(VisualServer::TextureFlags);
VARIANT_ENUM_CAST(VisualServer::ShaderMode);
VARIANT_ENUM_CAST(VisualServer::ArrayType);
VARIANT_ENUM_CAST(VisualServer::ArrayFormat);
VARIANT_ENUM_CAST(VisualServer::PrimitiveType);
VARIANT_ENUM_CAST(VisualServer::BlendShapeMode);
VARIANT_ENUM_CAST(VisualServer::LightType);
VARIANT_ENUM_CAST(VisualServer::LightParam);
VARIANT_ENUM_CAST(VisualServer::ViewportUpdateMode);
VARIANT_ENUM_CAST(VisualServer::ViewportClearMode);
VARIANT_ENUM_CAST(VisualServer::ViewportMSAA);
VARIANT_ENUM_CAST(VisualServer::ViewportUsage);
VARIANT_ENUM_CAST(VisualServer::ViewportRenderInfo);
VARIANT_ENUM_CAST(VisualServer::ViewportDebugDraw);
VARIANT_ENUM_CAST(VisualServer::ScenarioDebugMode);
VARIANT_ENUM_CAST(VisualServer::InstanceType);
VARIANT_ENUM_CAST(VisualServer::NinePatchAxisMode);
VARIANT_ENUM_CAST(VisualServer::CanvasLightMode);
VARIANT_ENUM_CAST(VisualServer::CanvasLightShadowFilter);
VARIANT_ENUM_CAST(VisualServer::CanvasOccluderPolygonCullMode);
VARIANT_ENUM_CAST(VisualServer::RenderInfo);
VARIANT_ENUM_CAST(VisualServer::Features);
VARIANT_ENUM_CAST(VisualServer::MultimeshTransformFormat);
VARIANT_ENUM_CAST(VisualServer::MultimeshColorFormat);
VARIANT_ENUM_CAST(VisualServer::MultimeshCustomDataFormat);
VARIANT_ENUM_CAST(VisualServer::LightOmniShadowMode);
VARIANT_ENUM_CAST(VisualServer::LightOmniShadowDetail);
VARIANT_ENUM_CAST(VisualServer::LightDirectionalShadowMode);
VARIANT_ENUM_CAST(VisualServer::LightDirectionalShadowDepthRangeMode);
VARIANT_ENUM_CAST(VisualServer::ReflectionProbeUpdateMode);
VARIANT_ENUM_CAST(VisualServer::ParticlesDrawOrder);
VARIANT_ENUM_CAST(VisualServer::EnvironmentBG);
VARIANT_ENUM_CAST(VisualServer::EnvironmentDOFBlurQuality);
VARIANT_ENUM_CAST(VisualServer::EnvironmentGlowBlendMode);
VARIANT_ENUM_CAST(VisualServer::EnvironmentToneMapper);
VARIANT_ENUM_CAST(VisualServer::EnvironmentSSAOQuality);
VARIANT_ENUM_CAST(VisualServer::EnvironmentSSAOBlur);
VARIANT_ENUM_CAST(VisualServer::InstanceFlags);
VARIANT_ENUM_CAST(VisualServer::ShadowCastingSetting);
VARIANT_ENUM_CAST(VisualServer::TextureType);
