/*************************************************************************/
/*  register_core_types.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "register_core_types.h"

#include "core/bind/core_bind.h"
#include "core/class_db.h"
#include "core/compressed_translation.h"
#include "core/core_string_names.h"
#include "core/crypto/crypto.h"
#include "core/crypto/hashing_context.h"
#include "core/engine.h"
//#include "core/func_ref.h"
#include "core/image.h"
#include "core/input/input_map.h"
#include "core/io/config_file.h"
#include "core/io/http_client.h"
#include "core/io/image_loader.h"
#include "core/io/marshalls.h"
#include "core/io/multiplayer_api.h"
#include "core/io/networked_multiplayer_peer.h"
#include "core/io/packet_peer.h"
#include "core/io/packet_peer_udp.h"
#include "core/io/pck_packer.h"
#include "core/io/resource_format_binary.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/io/stream_peer_ssl.h"
#include "core/io/tcp_server.h"
#include "core/io/translation_loader_po.h"
#include "core/io/xml_parser.h"
#include "core/math/a_star.h"
//#include "core/math/expression.h"
#include "core/math/geometry.h"
#include "core/math/random_number_generator.h"
#include "core/math/triangle_mesh.h"
#include "core/resource/manifest.h"
#include "core/object_db.h"
#include "core/os/input.h"
#include "core/os/main_loop.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/packed_data_container.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/translation.h"
#include "core/undo_redo.h"
#include "os/os.h"
#include "os/time.h"
#include "resource/resource_manager.h"

struct CodecStore {
    Ref<ResourceFormatSaverBinary> resource_saver_binary;
    Ref<ResourceFormatLoaderBinary> resource_loader_binary;
    Ref<ResourceFormatImporter> resource_format_importer;
    Ref<ResourceFormatLoaderImage> resource_format_image;
    Ref<TranslationLoaderPO> resource_format_po;
    Ref<ResourceFormatSaverCrypto> resource_format_saver_crypto;
    Ref<ResourceFormatLoaderCrypto> resource_format_loader_crypto;
};

static CodecStore *_codec_store = nullptr;
static _ResourceManager *_resource_manger = nullptr;
static _OS *_os = nullptr;
static _Engine *_engine = nullptr;
static _ClassDB *_classdb = nullptr;
static _Marshalls *_marshalls = nullptr;
static _JSON *_json = nullptr;

static IP *_ip = nullptr;

static _Geometry *_geometry = nullptr;

extern void register_global_constants();
extern void unregister_global_constants();

void register_core_types() {

    print_line("register_core_types");
    MemoryPool::setup();

    StringName::setup();
    gResourceManager().initialize();

    register_global_constants();

    CoreStringNames::create();

    TranslationLoaderPO::initialize_class();
    ResourceFormatSaverBinary::initialize_class();
    ResourceFormatLoaderBinary::initialize_class();
    ResourceFormatImporter::initialize_class();
    ResourceFormatLoaderImage::initialize_class();
    ResourceInteractiveLoaderDefault::initialize_class();

    _codec_store = memnew(CodecStore);
    _codec_store->resource_format_po = make_ref_counted<TranslationLoaderPO>();
    gResourceManager().add_resource_format_loader(_codec_store->resource_format_po);

    _codec_store->resource_saver_binary = make_ref_counted<ResourceFormatSaverBinary>();
    gResourceManager().add_resource_format_saver(_codec_store->resource_saver_binary);
    //TODO: SEGS: this is a hack to provide PNG resource saver
    gResourceManager().add_resource_format_saver(make_ref_counted<ResourceFormatSaver>());

    _codec_store->resource_loader_binary = make_ref_counted<ResourceFormatLoaderBinary>();
    gResourceManager().add_resource_format_loader(_codec_store->resource_loader_binary);

    _codec_store->resource_format_importer = make_ref_counted<ResourceFormatImporter>();
    gResourceManager().add_resource_format_loader(_codec_store->resource_format_importer);

    _codec_store->resource_format_image = make_ref_counted<ResourceFormatLoaderImage>();
    gResourceManager().add_resource_format_loader(_codec_store->resource_format_image);

    ClassDB::register_class<Object>();

    ClassDB::register_virtual_class<Script>();

    ClassDB::register_class<RefCounted>();
    ClassDB::register_class<WeakRef>();
    ClassDB::register_class<Resource>();
    ClassDB::register_class<Image>();
    ClassDB::register_class<ResourceManifest>();

    ClassDB::register_virtual_class<InputEvent>();
    ClassDB::register_virtual_class<InputEventWithModifiers>();
    ClassDB::register_class<InputEventKey>();
    ClassDB::register_virtual_class<InputEventMouse>();
    ClassDB::register_class<InputEventMouseButton>();
    ClassDB::register_class<InputEventMouseMotion>();
    ClassDB::register_class<InputEventJoypadButton>();
    ClassDB::register_class<InputEventJoypadMotion>();
    ClassDB::register_class<InputEventScreenDrag>();
    ClassDB::register_class<InputEventScreenTouch>();
    ClassDB::register_class<InputEventAction>();
    ClassDB::register_virtual_class<InputEventGesture>();
    ClassDB::register_class<InputEventMagnifyGesture>();
    ClassDB::register_class<InputEventPanGesture>();
    ClassDB::register_class<InputEventMIDI>();

    //ClassDB::register_class<FuncRef>();
    ClassDB::register_virtual_class<StreamPeer>();
    ClassDB::register_class<StreamPeerBuffer>();
    ClassDB::register_class<StreamPeerTCP>();
    ClassDB::register_class<TCP_Server>();
    ClassDB::register_class<PacketPeerUDP>();

    // Crypto
    ClassDB::register_class<HashingContext>();
    ClassDB::register_custom_instance_class<X509Certificate>();
    ClassDB::register_custom_instance_class<CryptoKey>();
    ClassDB::register_custom_instance_class<HMACContext>();
    ClassDB::register_custom_instance_class<Crypto>();
    ClassDB::register_custom_instance_class<StreamPeerSSL>();

    _codec_store->resource_format_saver_crypto = make_ref_counted<ResourceFormatSaverCrypto>();
    gResourceManager().add_resource_format_saver(_codec_store->resource_format_saver_crypto);
    _codec_store->resource_format_loader_crypto = make_ref_counted<ResourceFormatLoaderCrypto>();
    gResourceManager().add_resource_format_loader(_codec_store->resource_format_loader_crypto);

    ClassDB::register_virtual_class<IP>();
    ClassDB::register_virtual_class<PacketPeer>();
    ClassDB::register_class<PacketPeerStream>();
    ClassDB::register_virtual_class<NetworkedMultiplayerPeer>();
    ClassDB::register_class<MultiplayerAPI>();
    ClassDB::register_class<MainLoop>();
    ClassDB::register_class<Translation>();
    ClassDB::register_class<PHashTranslation>();
    ClassDB::register_class<UndoRedo>();
    ClassDB::register_class<HTTPClient>();
    ClassDB::register_class<TriangleMesh>();

    ClassDB::register_virtual_class<ResourceInteractiveLoader>();

    ClassDB::register_class<ResourceFormatLoader>();
    ClassDB::register_class<ResourceFormatSaver>();

    ClassDB::register_class<_File>();
    ClassDB::register_class<_Directory>();
    ClassDB::register_class<_Thread>();
    ClassDB::register_class<_Mutex>();
    ClassDB::register_class<_Semaphore>();

    ClassDB::register_class<XMLParser>();

    ClassDB::register_class<ConfigFile>();

    ClassDB::register_class<PCKPacker>();

    ClassDB::register_class<PackedDataContainer>();
    ClassDB::register_virtual_class<PackedDataContainerRef>();
    ClassDB::register_class<AStar>();
    ClassDB::register_class<AStar2D>();
    ClassDB::register_class<EncodedObjectAsID>();
    ClassDB::register_class<RandomNumberGenerator>();

    ClassDB::register_class<JSONParseResult>();

    ClassDB::register_virtual_class<ResourceImporter>();

    _ip = IP::create();
    _Geometry::initialize_class();
    _ResourceManager::initialize_class();
    _OS::initialize_class();
    _Engine::initialize_class();
    _ClassDB::initialize_class();
    _Marshalls::initialize_class();
    _JSON::initialize_class();

    _geometry = memnew(_Geometry);

    _resource_manger = memnew(_ResourceManager);
    _os = memnew(_OS);
    _engine = memnew(_Engine);
    _classdb = memnew(_ClassDB);
    _marshalls = memnew(_Marshalls);
    _json = memnew(_JSON);
}

void register_core_settings() {
    // Since in register core types, globals may not be present.
    GLOBAL_DEF("network/limits/tcp/connect_timeout_seconds", (30));
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/tcp/connect_timeout_seconds", PropertyInfo(VariantType::INT, "network/limits/tcp/connect_timeout_seconds", PropertyHint::Range, "1,1800,1"));
    GLOBAL_DEF_RST("network/limits/packet_peer_stream/max_buffer_po2", (16));
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/packet_peer_stream/max_buffer_po2", PropertyInfo(VariantType::INT, "network/limits/packet_peer_stream/max_buffer_po2", PropertyHint::Range, "0,64,1,or_greater"));

    GLOBAL_DEF("network/ssl/certificates", "");
    ProjectSettings::get_singleton()->set_custom_property_info("network/ssl/certificates", PropertyInfo(VariantType::STRING, "network/ssl/certificates", PropertyHint::File, "*.crt"));
}

void register_core_singletons() {

    ClassDB::register_class<ProjectSettings>();
    ClassDB::register_virtual_class<IP>();
    ClassDB::register_class<_Geometry>();
    ClassDB::register_class<_ResourceManager>();
    ClassDB::register_class<_OS>();
    ClassDB::register_class<_Engine>();
    ClassDB::register_class<_ClassDB>();
    ClassDB::register_class<_Marshalls>();
    ClassDB::register_class<TranslationServer>();
    ClassDB::register_virtual_class<Input>();
    ClassDB::register_class<InputMap>();
    ClassDB::register_class<_JSON>();
    //ClassDB::register_class<Expression>();
    ClassDB::register_class<Time>();

    Engine *en =Engine::get_singleton();
    en->add_singleton(Engine::Singleton(StaticCString("ProjectSettings"), ProjectSettings::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("IP"), IP::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("Geometry"), _Geometry::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("ResourceManager"), _ResourceManager::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("OS"), _OS::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("Engine"), _Engine::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("ClassDB"), _classdb));
    en->add_singleton(Engine::Singleton(StaticCString("Marshalls"), _Marshalls::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("TranslationServer"), TranslationServer::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("Input"), Input::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("InputMap"), InputMap::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("JSON"), _JSON::get_singleton()));
    en->add_singleton(Engine::Singleton(StaticCString("Time"), Time::get_singleton()));
}

void unregister_core_types() {

    memdelete(_resource_manger);
    memdelete(_os);
    memdelete(_engine);
    memdelete(_classdb);
    memdelete(_marshalls);
    memdelete(_json);

    memdelete(_geometry);

    gResourceManager().remove_resource_format_loader(_codec_store->resource_format_image);
    gResourceManager().remove_resource_format_saver(_codec_store->resource_saver_binary);
    gResourceManager().remove_resource_format_loader(_codec_store->resource_loader_binary);
    gResourceManager().remove_resource_format_loader(_codec_store->resource_format_importer);
    gResourceManager().remove_resource_format_loader(_codec_store->resource_format_po);
    gResourceManager().remove_resource_format_saver(_codec_store->resource_format_saver_crypto);
    gResourceManager().remove_resource_format_loader(_codec_store->resource_format_loader_crypto);

    memdelete(_ip);

    gResourceManager().finalize();
    memdelete(_codec_store);

    ClassDB::cleanup_defaults();
    ObjectDB::cleanup();

    unregister_global_constants();

    ClassDB::cleanup();
    ResourceCache::clear();
    CoreStringNames::free();
    StringName::cleanup(OS::get_singleton()->is_stdout_verbose());


    MemoryPool::cleanup();
}
