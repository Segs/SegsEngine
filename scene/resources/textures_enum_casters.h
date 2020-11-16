#pragma once

#include "scene/resources/texture.h"
#include "core/method_enum_caster.h"
#include "core/string_utils.inl"

VARIANT_ENUM_CAST(Texture::Flags);
VARIANT_ENUM_CAST(ImageTexture::Storage);
VARIANT_ENUM_CAST(CubeMap::Flags);
VARIANT_ENUM_CAST(CubeMap::Side);
VARIANT_ENUM_CAST(CubeMap::Storage);
VARIANT_ENUM_CAST(TextureLayered::Flags);
