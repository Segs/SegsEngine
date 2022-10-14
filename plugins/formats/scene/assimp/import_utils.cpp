#include "import_utils.h"

#include "core/resource/resource_manager.h"
#include "scene/animation/animation_player.h"

float AssimpUtils::get_fbx_fps(int32_t time_mode, const aiScene *p_scene) {
    switch (time_mode) {
    case AssetImportFbx::TIME_MODE_DEFAULT: return 24; //hack
    case AssetImportFbx::TIME_MODE_120: return 120;
    case AssetImportFbx::TIME_MODE_100: return 100;
    case AssetImportFbx::TIME_MODE_60: return 60;
    case AssetImportFbx::TIME_MODE_50: return 50;
    case AssetImportFbx::TIME_MODE_48: return 48;
    case AssetImportFbx::TIME_MODE_30: return 30;
    case AssetImportFbx::TIME_MODE_30_DROP: return 30;
    case AssetImportFbx::TIME_MODE_NTSC_DROP_FRAME: return 29.9700262f;
    case AssetImportFbx::TIME_MODE_NTSC_FULL_FRAME: return 29.9700262f;
    case AssetImportFbx::TIME_MODE_PAL: return 25;
    case AssetImportFbx::TIME_MODE_CINEMA: return 24;
    case AssetImportFbx::TIME_MODE_1000: return 1000;
    case AssetImportFbx::TIME_MODE_CINEMA_ND: return 23.976f;
    case AssetImportFbx::TIME_MODE_CUSTOM:
        int32_t frame_rate = -1;
        p_scene->mMetaData->Get("FrameRate", frame_rate);
        return frame_rate;
    }
    return 0;
}

void AssimpUtils::find_texture_path(StringView p_path, DirAccess *dir, String &path, bool &found, const String &extension) {
    FixedVector<String,32,true> paths;
    using namespace PathUtils;
    paths.emplace_back(String(get_basename(path)) + extension);
    paths.emplace_back(path + extension);
    paths.emplace_back(path);
    paths.emplace_back(plus_file(get_base_dir(p_path),String(get_basename(get_file(path))) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),String(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"textures/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"textures/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"textures/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Textures/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Textures/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Textures/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Textures/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Textures/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Textures/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../textures/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../textures/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../textures/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"texture/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"texture/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"texture/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Texture/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Texture/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"Texture/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Texture/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Texture/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../Texture/" + get_file(path)));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../texture/" + get_basename(get_file(path)) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../texture/" + get_file(path) + extension));
    paths.emplace_back(plus_file(get_base_dir(p_path),"../texture/" + get_file(path)));
    for (size_t i = 0; i < paths.size(); i++) {
        if (dir->file_exists(paths[i])) {
            found = true;
            path = paths[i];
            return;
        }
    }
}

void AssimpUtils::find_texture_path(const String &r_p_path, String &r_path, bool &r_found) {

    using namespace PathUtils;

    eastl::unique_ptr<DirAccess,wrap_deleter> dir(DirAccess::create(DirAccess::ACCESS_RESOURCES));
    Vector<String> exts;
    ImageLoader::get_recognized_extensions(exts);

    Vector<StringView> split_path = StringUtils::split(get_basename(r_path),'*');
    if (split_path.size() == 2) {
        r_found = true;
        return;
    }

    if (dir->file_exists(get_base_dir(r_p_path) + get_file(r_path))) {
        r_path = get_base_dir(r_p_path) + get_file(r_path);
        r_found = true;
        return;
    }

    for (auto & ext : exts) {
        if (r_found) {
            return;
        }
        find_texture_path(r_p_path, dir.get(), r_path, r_found, "." + ext);
    }
}

void AssimpUtils::set_texture_mapping_mode(aiTextureMapMode *map_mode, Ref<ImageTexture> texture) {
    ERR_FAIL_COND(not texture);
    ERR_FAIL_COND(map_mode == nullptr);
    aiTextureMapMode tex_mode = map_mode[0];

    int32_t flags = Texture::FLAGS_DEFAULT;
    if (tex_mode == aiTextureMapMode_Wrap) {
        //Default
    } else if (tex_mode == aiTextureMapMode_Clamp) {
        flags = flags & ~Texture::FLAG_REPEAT;
    } else if (tex_mode == aiTextureMapMode_Mirror) {
        flags = flags | Texture::FLAG_MIRRORED_REPEAT;
    }
    texture->set_flags(flags);
}

Ref<Image> AssimpUtils::load_image(ImportState &state, const aiScene *p_scene, StringView p_path) {
    using namespace PathUtils;
    Map<String, Ref<Image> >::iterator match = state.path_to_image_cache.find_as(p_path);

    // if our cache contains this image then don't bother
    if (match!=state.path_to_image_cache.end()) {
        return match->second;
    }

    Vector<StringView> split_path = StringUtils::split(get_basename(p_path),'*');
    if (split_path.size() == 2) {
        size_t texture_idx = StringUtils::to_int(split_path[1]);
        ERR_FAIL_COND_V(texture_idx >= p_scene->mNumTextures, Ref<Image>());
                aiTexture *tex = p_scene->mTextures[texture_idx];
        String filename = AssimpUtils::get_raw_string_from_assimp(tex->mFilename);
        filename = get_file(filename);
        print_verbose("Open Asset Import: Loading embedded texture " + filename);
        if (tex->mHeight == 0) {
            Ref<Image> img(make_ref_counted<Image>());
            Error e=img->load_from_buffer((uint8_t *)tex->pcData, tex->mWidth,tex->achFormatHint);
            ERR_FAIL_COND_V(e!=OK, Ref<Image>());
                    state.path_to_image_cache.emplace(String(p_path), img);
        } else {
            Ref<Image> img(make_ref_counted<Image>());
            PoolByteArray arr;
            uint32_t size = tex->mWidth * tex->mHeight;
            arr.resize(size);
            memcpy(arr.write().ptr(), tex->pcData, size);
            ERR_FAIL_COND_V(arr.size() % 4 != 0, Ref<Image>());
                    //ARGB8888 to RGBA8888
                    for (int32_t i = 0; i < arr.size() / 4; i++) {
                arr.write().ptr()[(4 * i) + 3] = arr[(4 * i) + 0];
                arr.write().ptr()[(4 * i) + 0] = arr[(4 * i) + 1];
                arr.write().ptr()[(4 * i) + 1] = arr[(4 * i) + 2];
                arr.write().ptr()[(4 * i) + 2] = arr[(4 * i) + 3];
            }
            img->create(tex->mWidth, tex->mHeight, true, ImageData::FORMAT_RGBA8, arr);
            ERR_FAIL_COND_V(not img, Ref<Image>());
            state.path_to_image_cache.emplace(String(p_path), img);
            return img;
        }
        return Ref<Image>();
    } else {
        RES loaded(gResourceManager().load(p_path));
        if(loaded) {
            Ref<Image> image;
            Ref<Texture> texture(dynamic_ref_cast<Texture>(loaded));
            ERR_FAIL_COND_V(not texture, Ref<Image>());
            image = texture->get_data();
            ERR_FAIL_COND_V(not image, Ref<Image>());
            state.path_to_image_cache.emplace(String(p_path), image);
            return image;
        }
    }

    return Ref<Image>();
}

bool AssimpUtils::CreateAssimpTexture(ImportState &state, const aiString &texture_path, String &filename, String &path, AssimpImageData &image_state) {
    using namespace PathUtils;
    filename = get_raw_string_from_assimp(texture_path);
    path = from_native_path(plus_file(get_base_dir(state.path),filename));
    bool found = false;
    find_texture_path(state.path, path, found);
    if (found) {
        image_state.raw_image = AssimpUtils::load_image(state, state.assimp_scene, path);
        if (image_state.raw_image) {
            image_state.texture=make_ref_counted<ImageTexture>();
            image_state.texture->create_from_image(image_state.raw_image);
            image_state.texture->set_storage(ImageTexture::STORAGE_COMPRESS_LOSSY);
            return true;
        }
    }

    return false;
}
