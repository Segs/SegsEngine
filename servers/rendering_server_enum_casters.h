#pragma once

#include "servers/rendering_server_enums.h"
#include "core/method_arg_casters.h"
#include "core/method_enum_caster.h"
#include "core/string_utils.h"

// make variant understand the enums
VARIANT_NS_ENUM_CAST(RS,CubeMapSide);
VARIANT_NS_ENUM_CAST(RS,TextureFlags);
VARIANT_NS_ENUM_CAST(RS,ShaderMode);
VARIANT_NS_ENUM_CAST(RS,ArrayType);
VARIANT_NS_ENUM_CAST(RS,ArrayFormat);
VARIANT_NS_ENUM_CAST(RS,PrimitiveType);
VARIANT_NS_ENUM_CAST(RS,BlendShapeMode);
VARIANT_NS_ENUM_CAST(RS,LightType);
VARIANT_NS_ENUM_CAST(RS,LightParam);
VARIANT_NS_ENUM_CAST(RS,ViewportUpdateMode);
VARIANT_NS_ENUM_CAST(RS,ViewportClearMode);
VARIANT_NS_ENUM_CAST(RS,ViewportMSAA);
VARIANT_NS_ENUM_CAST(RS,ViewportUsage);
VARIANT_NS_ENUM_CAST(RS,ViewportRenderInfo);
VARIANT_NS_ENUM_CAST(RS,ViewportDebugDraw);
VARIANT_NS_ENUM_CAST(RS,ScenarioDebugMode);
VARIANT_NS_ENUM_CAST(RS,InstanceType);
VARIANT_NS_ENUM_CAST(RS,NinePatchAxisMode);
VARIANT_NS_ENUM_CAST(RS,CanvasLightMode);
VARIANT_NS_ENUM_CAST(RS,CanvasLightShadowFilter);
VARIANT_NS_ENUM_CAST(RS,CanvasOccluderPolygonCullMode);
VARIANT_NS_ENUM_CAST(RS,RenderInfo);
VARIANT_NS_ENUM_CAST(RS,Features);
VARIANT_NS_ENUM_CAST(RS,MultimeshTransformFormat);
VARIANT_NS_ENUM_CAST(RS,MultimeshColorFormat);
VARIANT_NS_ENUM_CAST(RS,MultimeshCustomDataFormat);
VARIANT_NS_ENUM_CAST(RS,LightOmniShadowMode);
VARIANT_NS_ENUM_CAST(RS,LightOmniShadowDetail);
VARIANT_NS_ENUM_CAST(RS,LightDirectionalShadowMode);
VARIANT_NS_ENUM_CAST(RS,LightDirectionalShadowDepthRangeMode);
VARIANT_NS_ENUM_CAST(RS,ReflectionProbeUpdateMode);
VARIANT_NS_ENUM_CAST(RS,ParticlesDrawOrder);
VARIANT_NS_ENUM_CAST(RS,EnvironmentBG);
VARIANT_NS_ENUM_CAST(RS,EnvironmentDOFBlurQuality);
VARIANT_NS_ENUM_CAST(RS,EnvironmentGlowBlendMode);
VARIANT_NS_ENUM_CAST(RS,EnvironmentToneMapper);
VARIANT_NS_ENUM_CAST(RS,EnvironmentSSAOQuality);
VARIANT_NS_ENUM_CAST(RS,EnvironmentSSAOBlur);
VARIANT_NS_ENUM_CAST(RS,InstanceFlags);
VARIANT_NS_ENUM_CAST(RS,ShadowCastingSetting);
VARIANT_NS_ENUM_CAST(RS,TextureType);
