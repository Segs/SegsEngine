#pragma once

#include "servers/visual_server_enums.h"
#include "core/method_arg_casters.h"
#include "core/method_enum_caster.h"
#include "core/string_utils.h"

// make variant understand the enums
VARIANT_NS_ENUM_CAST(VS,CubeMapSide);
VARIANT_NS_ENUM_CAST(VS,TextureFlags);
VARIANT_NS_ENUM_CAST(VS,ShaderMode);
VARIANT_NS_ENUM_CAST(VS,ArrayType);
VARIANT_NS_ENUM_CAST(VS,ArrayFormat);
VARIANT_NS_ENUM_CAST(VS,PrimitiveType);
VARIANT_NS_ENUM_CAST(VS,BlendShapeMode);
VARIANT_NS_ENUM_CAST(VS,LightType);
VARIANT_NS_ENUM_CAST(VS,LightParam);
VARIANT_NS_ENUM_CAST(VS,ViewportUpdateMode);
VARIANT_NS_ENUM_CAST(VS,ViewportClearMode);
VARIANT_NS_ENUM_CAST(VS,ViewportMSAA);
VARIANT_NS_ENUM_CAST(VS,ViewportUsage);
VARIANT_NS_ENUM_CAST(VS,ViewportRenderInfo);
VARIANT_NS_ENUM_CAST(VS,ViewportDebugDraw);
VARIANT_NS_ENUM_CAST(VS,ScenarioDebugMode);
VARIANT_NS_ENUM_CAST(VS,InstanceType);
VARIANT_NS_ENUM_CAST(VS,NinePatchAxisMode);
VARIANT_NS_ENUM_CAST(VS,CanvasLightMode);
VARIANT_NS_ENUM_CAST(VS,CanvasLightShadowFilter);
VARIANT_NS_ENUM_CAST(VS,CanvasOccluderPolygonCullMode);
VARIANT_NS_ENUM_CAST(VS,RenderInfo);
VARIANT_NS_ENUM_CAST(VS,Features);
VARIANT_NS_ENUM_CAST(VS,MultimeshTransformFormat);
VARIANT_NS_ENUM_CAST(VS,MultimeshColorFormat);
VARIANT_NS_ENUM_CAST(VS,MultimeshCustomDataFormat);
VARIANT_NS_ENUM_CAST(VS,LightOmniShadowMode);
VARIANT_NS_ENUM_CAST(VS,LightOmniShadowDetail);
VARIANT_NS_ENUM_CAST(VS,LightDirectionalShadowMode);
VARIANT_NS_ENUM_CAST(VS,LightDirectionalShadowDepthRangeMode);
VARIANT_NS_ENUM_CAST(VS,ReflectionProbeUpdateMode);
VARIANT_NS_ENUM_CAST(VS,ParticlesDrawOrder);
VARIANT_NS_ENUM_CAST(VS,EnvironmentBG);
VARIANT_NS_ENUM_CAST(VS,EnvironmentDOFBlurQuality);
VARIANT_NS_ENUM_CAST(VS,EnvironmentGlowBlendMode);
VARIANT_NS_ENUM_CAST(VS,EnvironmentToneMapper);
VARIANT_NS_ENUM_CAST(VS,EnvironmentSSAOQuality);
VARIANT_NS_ENUM_CAST(VS,EnvironmentSSAOBlur);
VARIANT_NS_ENUM_CAST(VS,InstanceFlags);
VARIANT_NS_ENUM_CAST(VS,ShadowCastingSetting);
VARIANT_NS_ENUM_CAST(VS,TextureType);
