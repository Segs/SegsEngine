/*************************************************************************/
/*  register_scene_types.cpp                                             */
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

#include "register_scene_types.h"

#include "core/class_db.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/area_2d.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/2d/back_buffer_copy.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/canvas_item.h"
#include "scene/2d/canvas_item_material.h"
#include "scene/2d/canvas_modulate.h"
#include "scene/2d/collision_polygon_2d.h"
#include "scene/2d/collision_shape_2d.h"
#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/joints_2d.h"
#include "scene/2d/light_2d.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/line_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/multimesh_instance_2d.h"
#include "scene/2d/navigation_2d.h"
#include "scene/2d/navigation_agent_2d.h"
#include "scene/2d/navigation_obstacle_2d.h"
#include "scene/2d/parallax_background.h"
#include "scene/2d/parallax_layer.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/2d/path_2d.h"

#include "scene/2d/physics_body_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/2d/position_2d.h"
#include "scene/2d/ray_cast_2d.h"
#include "scene/2d/remote_transform_2d.h"
#include "scene/2d/skeleton_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/touch_screen_button.h"
#include "scene/2d/visibility_notifier_2d.h"
#include "scene/2d/y_sort.h"
#include "scene/animation/animation_cache.h"
#include "scene/animation/animation_blend_space_1d.h"
#include "scene/animation/animation_blend_space_2d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/animation/animation_tree_player.h"
#include "scene/animation/root_motion_view.h"
#include "scene/animation/tween.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/center_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/gradient_edit.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/graph_node.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/link_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/nine_patch_rect.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/reference_rect.h"
#include "scene/gui/rich_text_effect.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tabs.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_progress.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tool_button.h"
#include "scene/gui/tree.h"
#include "scene/gui/video_player.h"
#include "scene/gui/viewport_container.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/http_request.h"
#include "scene/main/instance_placeholder.h"
#include "scene/main/resource_preloader.h"
#include "scene/main/scene_tree.h"
#include "scene/main/timer.h"
#include "scene/main/viewport.h"
#include "scene/resources/audio_stream_sample.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/box_shape_3d.h"
#include "scene/resources/camera_texture.h"
#include "scene/resources/capsule_shape_3d.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape_3d.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_3d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/cylinder_shape_3d.h"
#include "scene/resources/default_theme/default_theme.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/gradient.h"
#include "scene/resources/height_map_shape_3d.h"
#include "scene/resources/line_shape_2d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/occluder_shape.h"
#include "scene/resources/occluder_shape_polygon.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/particles_material.h"
#include "scene/resources/physics_material.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/polygon_path_finder.h"
#include "scene/resources/primitive_meshes.h"
#include "scene/resources/ray_shape_3d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/resource_format_text.h"
#include "scene/resources/segment_shape_2d.h"
#include "scene/resources/sky.h"
#include "scene/resources/sphere_shape_3d.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/text_file.h"
#include "scene/resources/texture.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/tile_set.h"
#include "scene/resources/video_stream.h"
#include "scene/resources/visual_shader.h"
#include "scene/resources/visual_shader_nodes.h"
#include "scene/resources/world_3d.h"
#include "scene/resources/world_2d.h"
#include "scene/scene_string_names.h"

#ifndef _3D_DISABLED
#include "scene/3d/area_3d.h"
#include "scene/3d/arvr_nodes.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/baked_lightmap.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/collision_polygon_3d.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gi_probe.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/3d/immediate_geometry_3d.h"
#include "scene/3d/instantiation.h"
#include "scene/3d/interpolated_camera.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/listener_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/navigation_3d.h"
#include "scene/3d/navigation_agent.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/navigation_obstacle.h"
#include "scene/3d/path_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/3d/physics_joint_3d.h"
#include "scene/3d/portal.h"
#include "scene/3d/position_3d.h"
#include "scene/3d/proximity_group_3d.h"
#include "scene/3d/ray_cast_3d.h"
#include "scene/3d/reflection_probe.h"
#include "scene/3d/remote_transform_3d.h"
#include "scene/3d/room_instance.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/3d/soft_body_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/spring_arm_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/3d/vehicle_body_3d.h"
#include "scene/3d/visibility_notifier_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/animation/skeleton_ik_3d.h"
#include "scene/resources/environment.h"
#include "scene/resources/mesh_library.h"
#include "scene/resources/scene_library.h"

#include "scene/resources/font_serializers.h"
#include "scene/resources/texture_serializers.h"
#include "scene/resources/shader_serialization.h"

#endif

static Ref<ResourceFormatSaverText> resource_saver_text;
static Ref<ResourceFormatLoaderText> resource_loader_text;

static Ref<ResourceFormatLoaderDynamicFont> resource_loader_dynamic_font;

static Ref<ResourceFormatLoaderStreamTexture> resource_loader_stream_texture;
static Ref<ResourceFormatLoaderTextureLayered> resource_loader_texture_layered;

static Ref<ResourceFormatLoaderBMFont> resource_loader_bmfont;

static Ref<ResourceFormatSaverShader> resource_saver_shader;
static Ref<ResourceFormatLoaderShader> resource_loader_shader;

void register_scene_types() {

    SceneStringNames::create();

    OS::get_singleton()->yield(); //may take time to init

    Node::init_node_hrcr();

    AudioStreamPlayer::initialize_class();
    ResourcePreloader::initialize_class();
    HTTPRequest::initialize_class();
    Timer::initialize_class();
    SceneTree::initialize_class();
    ViewportTexture::initialize_class();
    Viewport::initialize_class();
    CanvasLayer::initialize_class();
    Node::initialize_class();
    register_viewport_local_classes();
    //RoomBounds::initialize_class();
    TileSet::initialize_class();
    LineShape2D::initialize_class();
    ConcavePolygonShape2D::initialize_class();
    DynamicFontData::initialize_class();
    DynamicFontAtSize::initialize_class();
    DynamicFont::initialize_class();
    ResourceFormatLoaderDynamicFont::initialize_class();
    ConvexPolygonShape2D::initialize_class();
    World2D::initialize_class();
    VisualShader::initialize_class();
    VisualShaderNodeCustom::initialize_class();
    VisualShaderNodeInput::initialize_class();
    VisualShaderNodeGroupBase::initialize_class();
    VisualShaderNodeExpression::initialize_class();
    VisualShaderNodeGlobalExpression::initialize_class();
    CapsuleShape2D::initialize_class();
    BoxShape3D::initialize_class();
    Gradient::initialize_class();
    ConvexPolygonShape3D::initialize_class();
    ResourceFormatLoaderShader::initialize_class();
    ResourceFormatSaverShader::initialize_class();
    SegmentShape2D::initialize_class();
    RayShape2D::initialize_class();
    ParticlesMaterial::initialize_class();
    CameraTexture::initialize_class();
    PolygonPathFinder::initialize_class();
    PlaneShape::initialize_class();
    MeshDataTool::initialize_class();
    SurfaceTool::initialize_class();
    ConcavePolygonShape3D::initialize_class();
    BitmapFont::initialize_class();
    ResourceFormatLoaderBMFont::initialize_class();
    AudioStreamPlaybackSample::initialize_class();
    AudioStreamSample::initialize_class();
    MultiMesh::initialize_class();
    BitMap::initialize_class();
    VisualShaderNodeScalarConstant::initialize_class();
    VisualShaderNodeBooleanConstant::initialize_class();
    VisualShaderNodeColorConstant::initialize_class();
    VisualShaderNodeVec3Constant::initialize_class();
    VisualShaderNodeTransformConstant::initialize_class();
    VisualShaderNodeTexture::initialize_class();
    VisualShaderNodeCubeMap::initialize_class();
    VisualShaderNodeScalarOp::initialize_class();
    VisualShaderNodeVectorOp::initialize_class();
    VisualShaderNodeColorOp::initialize_class();
    VisualShaderNodeTransformMult::initialize_class();
    VisualShaderNodeTransformVecMult::initialize_class();
    VisualShaderNodeScalarFunc::initialize_class();
    VisualShaderNodeVectorFunc::initialize_class();
    VisualShaderNodeColorFunc::initialize_class();
    VisualShaderNodeTransformFunc::initialize_class();
    VisualShaderNodeDotProduct::initialize_class();
    VisualShaderNodeVectorLen::initialize_class();
    VisualShaderNodeDeterminant::initialize_class();
    VisualShaderNodeScalarClamp::initialize_class();
    VisualShaderNodeVectorClamp::initialize_class();
    VisualShaderNodeScalarDerivativeFunc::initialize_class();
    VisualShaderNodeVectorDerivativeFunc::initialize_class();
    VisualShaderNodeFaceForward::initialize_class();
    VisualShaderNodeOuterProduct::initialize_class();
    VisualShaderNodeVectorScalarStep::initialize_class();
    VisualShaderNodeScalarSmoothStep::initialize_class();
    VisualShaderNodeVectorSmoothStep::initialize_class();
    VisualShaderNodeVectorScalarSmoothStep::initialize_class();
    VisualShaderNodeVectorDistance::initialize_class();
    VisualShaderNodeVectorRefract::initialize_class();
    VisualShaderNodeScalarInterp::initialize_class();
    VisualShaderNodeVectorInterp::initialize_class();
    VisualShaderNodeVectorScalarMix::initialize_class();
    VisualShaderNodeVectorCompose::initialize_class();
    VisualShaderNodeTransformCompose::initialize_class();
    VisualShaderNodeVectorDecompose::initialize_class();
    VisualShaderNodeTransformDecompose::initialize_class();
    VisualShaderNodeScalarUniform::initialize_class();
    VisualShaderNodeBooleanUniform::initialize_class();
    VisualShaderNodeColorUniform::initialize_class();
    VisualShaderNodeVec3Uniform::initialize_class();
    VisualShaderNodeTransformUniform::initialize_class();
    VisualShaderNodeTextureUniform::initialize_class();
    VisualShaderNodeTextureUniformTriplanar::initialize_class();
    VisualShaderNodeCubeMapUniform::initialize_class();
    VisualShaderNodeIf::initialize_class();
    VisualShaderNodeSwitch::initialize_class();
    VisualShaderNodeScalarSwitch::initialize_class();
    VisualShaderNodeFresnel::initialize_class();
    VisualShaderNodeIs::initialize_class();
    VisualShaderNodeCompare::initialize_class();
    PanoramaSky::initialize_class();
    ProceduralSky::initialize_class();
    PhysicsMaterial::initialize_class();
    CylinderShape3D::initialize_class();
    CircleShape2D::initialize_class();
    ArrayMesh::initialize_class();
    PackedScene::initialize_class();
    Environment::initialize_class();
    Curve::initialize_class();
    Curve2D::initialize_class();
    Curve3D::initialize_class();
    SphereShape3D::initialize_class();
    TextFile::initialize_class();
    World3D::initialize_class();
    MeshLibrary::initialize_class();
    SceneLibrary::initialize_class();
    CapsuleMesh::initialize_class();
    CubeMesh::initialize_class();
    CylinderMesh::initialize_class();
    PlaneMesh::initialize_class();
    PrismMesh::initialize_class();
    QuadMesh::initialize_class();
    SphereMesh::initialize_class();
    PointMesh::initialize_class();
    ResourceInteractiveLoaderText::initialize_class();
    ResourceFormatLoaderText::initialize_class();
    ResourceFormatSaverText::initialize_class();
    CapsuleShape3D::initialize_class();
    DynamicFontData::initialize_class();
    DynamicFontAtSize::initialize_class();
    DynamicFont::initialize_class();
    ResourceFormatLoaderDynamicFont::initialize_class();
    RayShape3D::initialize_class();
    Animation::initialize_class();
    VideoStreamPlayback::initialize_class();
    RectangleShape2D::initialize_class();
    HeightMapShape3D::initialize_class();
    CurveTexture::initialize_class();
    ShaderMaterial::initialize_class();
    SpatialMaterial::initialize_class();
    Theme::initialize_class();
    StyleBoxEmpty::initialize_class();
    StyleBoxTexture::initialize_class();
    StyleBoxFlat::initialize_class();
    StyleBoxLine::initialize_class();
    ResourceFormatLoaderStreamTexture::initialize_class();
    AtlasTexture::initialize_class();
    MeshTexture::initialize_class();
    LargeTexture::initialize_class();
    CubeMap::initialize_class();
    Texture3D::initialize_class();
    TextureArray::initialize_class();
    ResourceFormatLoaderTextureLayered::initialize_class();
    GradientTexture::initialize_class();
    ProxyTexture::initialize_class();
    AnimatedTexture::initialize_class();
    Navigation2D::initialize_class();
    Area2D::initialize_class();
    NavigationPolygon::initialize_class();
    NavigationPolygonInstance::initialize_class();
    VisibilityNotifier2D::initialize_class();
    VisibilityEnabler2D::initialize_class();
    CollisionPolygon2D::initialize_class();
    Node2D::initialize_class();
    YSort::initialize_class();
    ParallaxBackground::initialize_class();
    GPUParticles2D::initialize_class();
    PinJoint2D::initialize_class();
    GrooveJoint2D::initialize_class();
    DampedSpringJoint2D::initialize_class();
    CPUParticles2D::initialize_class();
    Light2D::initialize_class();
    Position2D::initialize_class();
    CollisionShape2D::initialize_class();
    Bone2D::initialize_class();
    Skeleton2D::initialize_class();
    Line2D::initialize_class();
    RemoteTransform2D::initialize_class();
    MultiMeshInstance2D::initialize_class();
    OccluderPolygon2D::initialize_class();
    LightOccluder2D::initialize_class();
    Path2D::initialize_class();
    PathFollow2D::initialize_class();
    CanvasModulate::initialize_class();
    SpriteFrames::initialize_class();
    AnimatedSprite2D::initialize_class();
    MeshInstance2D::initialize_class();
    StaticBody2D::initialize_class();
    RigidBody2D::initialize_class();
    KinematicBody2D::initialize_class();
    KinematicCollision2D::initialize_class();
    RayCast2D::initialize_class();
    ParallaxLayer::initialize_class();
    TileMap::initialize_class();
    Polygon2D::initialize_class();
    Sprite2D::initialize_class();
    TouchScreenButton::initialize_class();
    CanvasItemMaterial::initialize_class();
    BackBufferCopy::initialize_class();
    Camera2D::initialize_class();
    AudioStreamPlayer2D::initialize_class();
    GraphEdit::initialize_class();
    GraphNode::initialize_class();
    HSplitContainer::initialize_class();
    VSplitContainer::initialize_class();
    HSlider::initialize_class();
    VSlider::initialize_class();
    Panel::initialize_class();
    TextureProgress::initialize_class();
    TabContainer::initialize_class();
    Tree::initialize_class();
    MenuButton::initialize_class();
    LinkButton::initialize_class();
    CenterContainer::initialize_class();
    CheckButton::initialize_class();
    Container::initialize_class();
    ToolButton::initialize_class();
    NinePatchRect::initialize_class();
    Control::initialize_class();
    FileDialog::initialize_class();
    LineEditFileChooser::initialize_class();
    ColorPicker::initialize_class();
    ColorPickerButton::initialize_class();
    Tabs::initialize_class();
    ReferenceRect::initialize_class();
    GradientEdit::initialize_class();
    GridContainer::initialize_class();
    ViewportContainer::initialize_class();
    ItemList::initialize_class();
    ColorRect::initialize_class();
    VideoPlayer::initialize_class();
    HBoxContainer::initialize_class();
    VBoxContainer::initialize_class();
    Button::initialize_class();
    CheckBox::initialize_class();
    OptionButton::initialize_class();
    RichTextEffect::initialize_class();
    CharFXTransform::initialize_class();
    VSeparator::initialize_class();
    HSeparator::initialize_class();
    ShortCut::initialize_class();
    PopupMenu::initialize_class();
    Label::initialize_class();
    TextureButton::initialize_class();
    TextureRect::initialize_class();
    ButtonGroup::initialize_class();
    Popup::initialize_class();
    PopupPanel::initialize_class();
    HScrollBar::initialize_class();
    VScrollBar::initialize_class();
    SpinBox::initialize_class();
    TextEdit::initialize_class();
    ProgressBar::initialize_class();
    PanelContainer::initialize_class();
    ScrollContainer::initialize_class();
    MarginContainer::initialize_class();
    WindowDialog::initialize_class();
    PopupDialog::initialize_class();
    AcceptDialog::initialize_class();
    ConfirmationDialog::initialize_class();
    RichTextLabel::initialize_class();
    LineEdit::initialize_class();
    Camera3D::initialize_class();
    ClippedCamera3D::initialize_class();
    Skeleton::initialize_class();
    VisibilityNotifier3D::initialize_class();
    VisibilityEnabler3D::initialize_class();
    Area3D::initialize_class();
    ARVRCamera::initialize_class();
    ARVRController::initialize_class();
    ARVRAnchor::initialize_class();
    ARVROrigin::initialize_class();
    GPUParticles3D::initialize_class();
    InterpolatedCamera::initialize_class();
    ProximityGroup3D::initialize_class();
    PinJoint3D::initialize_class();
    HingeJoint3D::initialize_class();
    SliderJoint3D::initialize_class();
    ConeTwistJoint3D::initialize_class();
    Generic6DOFJoint3D::initialize_class();
    Node3D::initialize_class();
    BakedLightmapData::initialize_class();
    BakedLightmap::initialize_class();
    VehicleWheel3D::initialize_class();
    VehicleBody3D::initialize_class();
    CollisionPolygon3D::initialize_class();
    DirectionalLight3D::initialize_class();
    OmniLight3D::initialize_class();
    SpotLight3D::initialize_class();
    Position3D::initialize_class();

    SoftBody3D::initialize_class();
    Listener3D::initialize_class();
    SpringArm3D::initialize_class();
    WorldEnvironment::initialize_class();
    CollisionShape3D::initialize_class();
    BoneAttachment3D::initialize_class();
    Sprite3D::initialize_class();
    AnimatedSprite3D::initialize_class();
    MeshInstance3D::initialize_class();
    ImmediateGeometry3D::initialize_class();
    AudioStreamPlayer3D::initialize_class();
    MultiMeshInstance3D::initialize_class();
    LibraryEntryInstance::initialize_class();
    RemoteTransform3D::initialize_class();
    StaticBody3D::initialize_class();
    RigidBody::initialize_class();
    KinematicBody3D::initialize_class();
    KinematicCollision::initialize_class();
    PhysicalBone3D::initialize_class();
    CPUParticles3D::initialize_class();
    GIProbeData::initialize_class();
    GIProbe::initialize_class();
//    Portal::initialize_class();
//    Room::initialize_class();
    RayCast3D::initialize_class();
    ReflectionProbe::initialize_class();
    VelocityTracker3D::initialize_class();
    Path3D::initialize_class();
    PathFollow3D::initialize_class();
    AnimationCache::initialize_class();
    AnimationTreePlayer::initialize_class();
    AnimationNodeAnimation::initialize_class();
    AnimationNodeOneShot::initialize_class();
    AnimationNodeAdd2::initialize_class();
    AnimationNodeAdd3::initialize_class();
    AnimationNodeBlend2::initialize_class();
    AnimationNodeBlend3::initialize_class();
    AnimationNodeTimeScale::initialize_class();
    AnimationNodeTimeSeek::initialize_class();
    AnimationNodeTransition::initialize_class();
    AnimationNodeOutput::initialize_class();
    AnimationNodeBlendTree::initialize_class();
    AnimationNodeBlendSpace1D::initialize_class();
    SkeletonIK3D::initialize_class();
    RootMotionView::initialize_class();
    AnimationPlayer::initialize_class();
    AnimationNodeStateMachineTransition::initialize_class();
    AnimationNodeStateMachinePlayback::initialize_class();
    AnimationNodeStateMachine::initialize_class();
    AnimationNode::initialize_class();
    AnimationRootNode::initialize_class();
    AnimationTree::initialize_class();
    Tween::initialize_class();
    AnimationNodeBlendSpace2D::initialize_class();



    resource_loader_dynamic_font = make_ref_counted<ResourceFormatLoaderDynamicFont>();
    gResourceManager().add_resource_format_loader(resource_loader_dynamic_font);

    resource_loader_stream_texture = make_ref_counted<ResourceFormatLoaderStreamTexture>();
    gResourceManager().add_resource_format_loader(resource_loader_stream_texture);

    resource_loader_texture_layered = make_ref_counted<ResourceFormatLoaderTextureLayered>();
    gResourceManager().add_resource_format_loader(resource_loader_texture_layered);

    resource_saver_text = make_ref_counted<ResourceFormatSaverText>();
    gResourceManager().add_resource_format_saver(resource_saver_text, true);

    resource_loader_text = make_ref_counted<ResourceFormatLoaderText>();
    gResourceManager().add_resource_format_loader(resource_loader_text, true);

    resource_saver_shader = make_ref_counted<ResourceFormatSaverShader>();
    gResourceManager().add_resource_format_saver(resource_saver_shader, true);

    resource_loader_shader = make_ref_counted<ResourceFormatLoaderShader>();
    gResourceManager().add_resource_format_loader(resource_loader_shader, true);

    resource_loader_bmfont = make_ref_counted<ResourceFormatLoaderBMFont>();
    gResourceManager().add_resource_format_loader(resource_loader_bmfont, true);

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<Object>();

    ClassDB::register_class<Node>();
    ClassDB::register_virtual_class<InstancePlaceholder>();

    ClassDB::register_class<Viewport>();
    ClassDB::register_class<ViewportTexture>();
    ClassDB::register_class<HTTPRequest>();
    ClassDB::register_class<Timer>();
    ClassDB::register_class<CanvasLayer>();
    ClassDB::register_class<CanvasModulate>();
    ClassDB::register_class<ResourcePreloader>();

    /* REGISTER GUI */
    ClassDB::register_class<ButtonGroup>();
    ClassDB::register_virtual_class<BaseButton>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<ShortCut>();
    ClassDB::register_class<Control>();
    ClassDB::register_class<Button>();
    ClassDB::register_class<Label>();
    ClassDB::register_virtual_class<ScrollBar>();
    ClassDB::register_class<HScrollBar>();
    ClassDB::register_class<VScrollBar>();
    ClassDB::register_class<ProgressBar>();
    ClassDB::register_virtual_class<Slider>();
    ClassDB::register_class<HSlider>();
    ClassDB::register_class<VSlider>();
    ClassDB::register_class<Popup>();
    ClassDB::register_class<PopupPanel>();
    ClassDB::register_class<MenuButton>();
    ClassDB::register_class<CheckBox>();
    ClassDB::register_class<CheckButton>();
    ClassDB::register_class<ToolButton>();
    ClassDB::register_class<LinkButton>();
    ClassDB::register_class<Panel>();
    ClassDB::register_virtual_class<Range>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<TextureRect>();
    ClassDB::register_class<ColorRect>();
    ClassDB::register_class<NinePatchRect>();
    ClassDB::register_class<ReferenceRect>();
    ClassDB::register_class<TabContainer>();
    ClassDB::register_class<Tabs>();
    ClassDB::register_virtual_class<Separator>();
    ClassDB::register_class<HSeparator>();
    ClassDB::register_class<VSeparator>();
    ClassDB::register_class<TextureButton>();
    ClassDB::register_class<Container>();
    ClassDB::register_virtual_class<BoxContainer>();
    ClassDB::register_class<HBoxContainer>();
    ClassDB::register_class<VBoxContainer>();
    ClassDB::register_class<GridContainer>();
    ClassDB::register_class<CenterContainer>();
    ClassDB::register_class<ScrollContainer>();
    ClassDB::register_class<PanelContainer>();
    ClassDB::register_virtual_class<FlowContainer>();
    ClassDB::register_class<HFlowContainer>();
    ClassDB::register_class<VFlowContainer>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<TextureProgress>();
    ClassDB::register_class<ItemList>();

    ClassDB::register_class<LineEdit>();
    ClassDB::register_class<VideoPlayer>();

#ifndef ADVANCED_GUI_DISABLED

    ClassDB::register_class<FileDialog>();

    ClassDB::register_class<PopupMenu>();
    ClassDB::register_class<Tree>();

    ClassDB::register_class<TextEdit>();

    ClassDB::register_virtual_class<TreeItem>();
    ClassDB::register_class<OptionButton>();
    ClassDB::register_class<SpinBox>();
    ClassDB::register_class<ColorPicker>();
    ClassDB::register_class<ColorPickerButton>();
    ClassDB::register_class<RichTextLabel>();
    ClassDB::register_class<RichTextEffect>();
    ClassDB::register_class<CharFXTransform>();
    ClassDB::register_class<PopupDialog>();
    ClassDB::register_class<WindowDialog>();
    ClassDB::register_class<AcceptDialog>();
    ClassDB::register_class<ConfirmationDialog>();
    ClassDB::register_class<MarginContainer>();
    ClassDB::register_class<ViewportContainer>();
    ClassDB::register_virtual_class<SplitContainer>();
    ClassDB::register_class<HSplitContainer>();
    ClassDB::register_class<VSplitContainer>();
    ClassDB::register_class<GraphNode>();
    GraphEditFilter::initialize_class();
    GraphEditMinimap::initialize_class();
    ClassDB::register_class<GraphEdit>();

    OS::get_singleton()->yield(); //may take time to init

#endif

    /* REGISTER 3D */

    ClassDB::register_class<Skin>();
    ClassDB::register_virtual_class<SkinReference>();

    ClassDB::register_class<Node3D>();
    ClassDB::register_virtual_class<Node3DGizmo>();
    ClassDB::register_class<Skeleton>();
    ClassDB::register_class<AnimationPlayer>();
    ClassDB::register_class<Tween>();

    ClassDB::register_class<AnimationTreePlayer>();
    ClassDB::register_class<AnimationTree>();
    ClassDB::register_class<AnimationNode>();
    ClassDB::register_class<AnimationRootNode>();
    ClassDB::register_class<AnimationNodeBlendTree>();
    ClassDB::register_class<AnimationNodeBlendSpace1D>();
    ClassDB::register_class<AnimationNodeBlendSpace2D>();
    ClassDB::register_class<AnimationNodeStateMachine>();
    ClassDB::register_class<AnimationNodeStateMachinePlayback>();

    ClassDB::register_class<AnimationNodeStateMachineTransition>();
    ClassDB::register_class<AnimationNodeOutput>();
    ClassDB::register_class<AnimationNodeOneShot>();
    ClassDB::register_class<AnimationNodeAnimation>();
    ClassDB::register_class<AnimationNodeAdd2>();
    ClassDB::register_class<AnimationNodeAdd3>();
    ClassDB::register_class<AnimationNodeBlend2>();
    ClassDB::register_class<AnimationNodeBlend3>();
    ClassDB::register_class<AnimationNodeTimeScale>();
    ClassDB::register_class<AnimationNodeTimeSeek>();
    ClassDB::register_class<AnimationNodeTransition>();

    OS::get_singleton()->yield(); //may take time to init

#ifndef _3D_DISABLED
    ClassDB::register_virtual_class<VisualInstance3D>();
    ClassDB::register_virtual_class<GeometryInstance>();
    ClassDB::register_class<Camera3D>();
    ClassDB::register_class<ClippedCamera3D>();
    ClassDB::register_class<Listener3D>();
    ClassDB::register_class<ARVRCamera>();
    ClassDB::register_class<ARVRController>();
    ClassDB::register_class<ARVRAnchor>();
    ClassDB::register_class<ARVROrigin>();
    ClassDB::register_class<InterpolatedCamera>();
    ClassDB::register_class<MeshInstance3D>();
    ClassDB::register_class<LibraryEntryInstance>();
    ClassDB::register_class<ImmediateGeometry3D>();
    ClassDB::register_virtual_class<SpriteBase3D>();
    ClassDB::register_class<Sprite3D>();
    ClassDB::register_class<AnimatedSprite3D>();
    ClassDB::register_virtual_class<Light3D>();
    ClassDB::register_class<DirectionalLight3D>();
    ClassDB::register_class<OmniLight3D>();
    ClassDB::register_class<SpotLight3D>();
    ClassDB::register_class<ReflectionProbe>();
    ClassDB::register_class<GIProbe>();
    ClassDB::register_class<GIProbeData>();
    ClassDB::register_class<BakedLightmap>();
    ClassDB::register_class<BakedLightmapData>();
    ClassDB::register_class<GPUParticles3D>();
    ClassDB::register_class<CPUParticles3D>();
    ClassDB::register_class<Position3D>();

    ClassDB::register_class<RootMotionView>();
    ClassDB::set_class_enabled("RootMotionView", false); //disabled by default, enabled by editor

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_virtual_class<CollisionObject3D>();
    ClassDB::register_virtual_class<PhysicsBody3D>();
    ClassDB::register_class<StaticBody3D>();
    ClassDB::register_class<RigidBody>();
    ClassDB::register_class<KinematicCollision>();
    ClassDB::register_class<KinematicBody3D>();
    ClassDB::register_class<SpringArm3D>();

    ClassDB::register_class<PhysicalBone3D>();
    ClassDB::register_class<SoftBody3D>();

    ClassDB::register_class<SkeletonIK3D>();
    ClassDB::register_class<BoneAttachment3D>();

    ClassDB::register_class<VehicleBody3D>();
    ClassDB::register_class<VehicleWheel3D>();
    ClassDB::register_class<Area3D>();
    ClassDB::register_class<ProximityGroup3D>();
    ClassDB::register_class<CollisionShape3D>();
    ClassDB::register_class<CollisionPolygon3D>();
    ClassDB::register_class<RayCast3D>();
    ClassDB::register_class<MultiMeshInstance3D>();

    ClassDB::register_class<Curve3D>();
    ClassDB::register_class<Path3D>();
    ClassDB::register_class<PathFollow3D>();
    ClassDB::register_class<VisibilityNotifier3D>();
    ClassDB::register_class<VisibilityEnabler3D>();
    ClassDB::register_class<WorldEnvironment>();
    ClassDB::register_class<RemoteTransform3D>();

    ClassDB::register_virtual_class<Joint3D>();
    ClassDB::register_class<PinJoint3D>();
    ClassDB::register_class<HingeJoint3D>();
    ClassDB::register_class<SliderJoint3D>();
    ClassDB::register_class<ConeTwistJoint3D>();
    ClassDB::register_class<Generic6DOFJoint3D>();

    ClassDB::register_class<Navigation3D>();
    ClassDB::register_class<NavigationMeshInstance>();
    ClassDB::register_class<NavigationAgent>();
    ClassDB::register_class<NavigationObstacle>();

    OS::get_singleton()->yield(); //may take time to init

#endif
    ClassDB::register_class<NavigationMesh>();

    AcceptDialog::set_swap_ok_cancel(T_GLOBAL_DEF("gui/common/swap_ok_cancel", bool(OS::get_singleton()->get_swap_ok_cancel())));

    ClassDB::register_class<Shader>();
    ClassDB::register_class<VisualShader>();
    ClassDB::register_virtual_class<VisualShaderNode>();
    ClassDB::register_class<VisualShaderNodeCustom>();
    ClassDB::register_class<VisualShaderNodeInput>();
    ClassDB::register_virtual_class<VisualShaderNodeOutput>();
    ClassDB::register_class<VisualShaderNodeGroupBase>();
    ClassDB::register_class<VisualShaderNodeScalarConstant>();
    ClassDB::register_class<VisualShaderNodeBooleanConstant>();
    ClassDB::register_class<VisualShaderNodeColorConstant>();
    ClassDB::register_class<VisualShaderNodeVec3Constant>();
    ClassDB::register_class<VisualShaderNodeTransformConstant>();
    ClassDB::register_class<VisualShaderNodeScalarOp>();
    ClassDB::register_class<VisualShaderNodeVectorOp>();
    ClassDB::register_class<VisualShaderNodeColorOp>();
    ClassDB::register_class<VisualShaderNodeTransformMult>();
    ClassDB::register_class<VisualShaderNodeTransformVecMult>();
    ClassDB::register_class<VisualShaderNodeScalarFunc>();
    ClassDB::register_class<VisualShaderNodeVectorFunc>();
    ClassDB::register_class<VisualShaderNodeColorFunc>();
    ClassDB::register_class<VisualShaderNodeTransformFunc>();
    ClassDB::register_class<VisualShaderNodeDotProduct>();
    ClassDB::register_class<VisualShaderNodeVectorLen>();
    ClassDB::register_class<VisualShaderNodeDeterminant>();
    ClassDB::register_class<VisualShaderNodeScalarDerivativeFunc>();
    ClassDB::register_class<VisualShaderNodeVectorDerivativeFunc>();
    ClassDB::register_class<VisualShaderNodeScalarClamp>();
    ClassDB::register_class<VisualShaderNodeVectorClamp>();
    ClassDB::register_class<VisualShaderNodeFaceForward>();
    ClassDB::register_class<VisualShaderNodeOuterProduct>();
    ClassDB::register_class<VisualShaderNodeVectorScalarStep>();
    ClassDB::register_class<VisualShaderNodeScalarSmoothStep>();
    ClassDB::register_class<VisualShaderNodeVectorSmoothStep>();
    ClassDB::register_class<VisualShaderNodeVectorScalarSmoothStep>();
    ClassDB::register_class<VisualShaderNodeVectorDistance>();
    ClassDB::register_class<VisualShaderNodeVectorRefract>();
    ClassDB::register_class<VisualShaderNodeScalarInterp>();
    ClassDB::register_class<VisualShaderNodeVectorInterp>();
    ClassDB::register_class<VisualShaderNodeVectorScalarMix>();
    ClassDB::register_class<VisualShaderNodeVectorCompose>();
    ClassDB::register_class<VisualShaderNodeTransformCompose>();
    ClassDB::register_class<VisualShaderNodeVectorDecompose>();
    ClassDB::register_class<VisualShaderNodeTransformDecompose>();
    ClassDB::register_class<VisualShaderNodeTexture>();
    ClassDB::register_class<VisualShaderNodeCubeMap>();
    ClassDB::register_virtual_class<VisualShaderNodeUniform>();
    ClassDB::register_class<VisualShaderNodeUniformRef>();
    ClassDB::register_class<VisualShaderNodeScalarUniform>();
    ClassDB::register_class<VisualShaderNodeBooleanUniform>();
    ClassDB::register_class<VisualShaderNodeColorUniform>();
    ClassDB::register_class<VisualShaderNodeVec3Uniform>();
    ClassDB::register_class<VisualShaderNodeTransformUniform>();
    ClassDB::register_class<VisualShaderNodeTextureUniform>();
    ClassDB::register_class<VisualShaderNodeTextureUniformTriplanar>();
    ClassDB::register_class<VisualShaderNodeCubeMapUniform>();
    ClassDB::register_class<VisualShaderNodeIf>();
    ClassDB::register_class<VisualShaderNodeSwitch>();
    ClassDB::register_class<VisualShaderNodeScalarSwitch>();
    ClassDB::register_class<VisualShaderNodeFresnel>();
    ClassDB::register_class<VisualShaderNodeExpression>();
    ClassDB::register_class<VisualShaderNodeGlobalExpression>();
    ClassDB::register_class<VisualShaderNodeIs>();
    ClassDB::register_class<VisualShaderNodeCompare>();

    ClassDB::register_class<ShaderMaterial>();
    ClassDB::register_virtual_class<CanvasItem>();
    ClassDB::register_class<CanvasItemMaterial>();
    SceneTree::add_idle_callback(CanvasItemMaterial::flush_changes);
    CanvasItemMaterial::init_shaders();
    ClassDB::register_class<Node2D>();
    ClassDB::register_class<CPUParticles2D>();
    ClassDB::register_class<GPUParticles2D>();
    //ClassDB::register_class<ParticleAttractor2D>();
    ClassDB::register_class<Sprite2D>();
    //ClassDB::register_type<ViewportSprite>();
    ClassDB::register_class<SpriteFrames>();
    ClassDB::register_class<AnimatedSprite2D>();
    ClassDB::register_class<Position2D>();
    ClassDB::register_class<Line2D>();
    ClassDB::register_class<MeshInstance2D>();
    ClassDB::register_class<MultiMeshInstance2D>();
    ClassDB::register_virtual_class<CollisionObject2D>();
    ClassDB::register_virtual_class<PhysicsBody2D>();
    ClassDB::register_class<StaticBody2D>();
    ClassDB::register_class<RigidBody2D>();
    ClassDB::register_class<KinematicBody2D>();
    ClassDB::register_class<KinematicCollision2D>();
    ClassDB::register_class<Area2D>();
    ClassDB::register_class<CollisionShape2D>();
    ClassDB::register_class<CollisionPolygon2D>();
    ClassDB::register_class<RayCast2D>();
    ClassDB::register_class<VisibilityNotifier2D>();
    ClassDB::register_class<VisibilityEnabler2D>();
    ClassDB::register_class<Polygon2D>();
    ClassDB::register_class<Skeleton2D>();
    ClassDB::register_class<Bone2D>();
    ClassDB::register_class<Light2D>();
    ClassDB::register_class<LightOccluder2D>();
    ClassDB::register_class<OccluderPolygon2D>();
    ClassDB::register_class<YSort>();
    ClassDB::register_class<BackBufferCopy>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<Camera2D>();
    ClassDB::register_virtual_class<Joint2D>();
    ClassDB::register_class<PinJoint2D>();
    ClassDB::register_class<GrooveJoint2D>();
    ClassDB::register_class<DampedSpringJoint2D>();
    ClassDB::register_class<TileSet>();
    ClassDB::register_class<TileMap>();
    ClassDB::register_class<ParallaxBackground>();
    ClassDB::register_class<ParallaxLayer>();
    ClassDB::register_class<TouchScreenButton>();
    ClassDB::register_class<RemoteTransform2D>();

    OS::get_singleton()->yield(); //may take time to init

    /* REGISTER RESOURCES */

    ClassDB::register_virtual_class<Shader>();
    ClassDB::register_class<ParticlesMaterial>();
    SceneTree::add_idle_callback(ParticlesMaterial::flush_changes);
    ParticlesMaterial::init_shaders();

    ClassDB::register_virtual_class<Mesh>();
    ClassDB::register_class<ArrayMesh>();
    ClassDB::register_class<MultiMesh>();
    ClassDB::register_class<SurfaceTool>();
    ClassDB::register_class<MeshDataTool>();

#ifndef _3D_DISABLED
    ClassDB::register_virtual_class<PrimitiveMesh>();
    ClassDB::register_class<CapsuleMesh>();
    ClassDB::register_class<CubeMesh>();
    ClassDB::register_class<CylinderMesh>();
    ClassDB::register_class<PlaneMesh>();
    ClassDB::register_class<PrismMesh>();
    ClassDB::register_class<QuadMesh>();
    ClassDB::register_class<SphereMesh>();
    ClassDB::register_class<PointMesh>();
    ClassDB::register_virtual_class<Material>();
    ClassDB::register_class<SpatialMaterial>();
    SceneTree::add_idle_callback(SpatialMaterial::flush_changes);
    SpatialMaterial::init_shaders();

    ClassDB::register_class<MeshLibrary>();
    ClassDB::register_class<SceneLibrary>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_virtual_class<Shape>();
    ClassDB::register_class<RayShape3D>();
    ClassDB::register_class<SphereShape3D>();
    ClassDB::register_class<BoxShape3D>();
    ClassDB::register_class<CapsuleShape3D>();
    ClassDB::register_class<CylinderShape3D>();
    ClassDB::register_class<HeightMapShape3D>();
    ClassDB::register_class<PlaneShape>();
    ClassDB::register_class<ConvexPolygonShape3D>();
    ClassDB::register_class<ConcavePolygonShape3D>();
    ClassDB::register_virtual_class<OccluderShape>();
    ClassDB::register_class<OccluderShapeSphere>();
    ClassDB::register_class<OccluderShapePolygon>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<VelocityTracker3D>();

#endif
    ClassDB::register_class<PhysicsMaterial>();
    ClassDB::register_class<World3D>();
    ClassDB::register_class<Environment>();
    ClassDB::register_class<World2D>();
    ClassDB::register_virtual_class<Texture>();
    ClassDB::register_virtual_class<Sky>();
    ClassDB::register_class<PanoramaSky>();
    ClassDB::register_class<ProceduralSky>();
    ClassDB::register_class<StreamTexture>();
    ClassDB::register_class<ImageTexture>();
    ClassDB::register_class<AtlasTexture>();
    ClassDB::register_class<MeshTexture>();
    ClassDB::register_class<LargeTexture>();
    ClassDB::register_class<CurveTexture>();
    ClassDB::register_class<GradientTexture>();
    ClassDB::register_class<GradientTexture2D>();
    ClassDB::register_class<ProxyTexture>();
    ClassDB::register_class<AnimatedTexture>();
    ClassDB::register_class<CameraTexture>();
    ClassDB::register_class<ExternalTexture>();
    ClassDB::register_class<CubeMap>();
    ClassDB::register_virtual_class<TextureLayered>();
    ClassDB::register_class<Texture3D>();
    ClassDB::register_class<TextureArray>();
    ClassDB::register_class<Animation>();
    ClassDB::register_virtual_class<Font>();
    ClassDB::register_class<BitmapFont>();
    ClassDB::register_class<Curve>();

    ClassDB::register_class<TextFile>();

    ClassDB::register_class<DynamicFontData>();
    ClassDB::register_class<DynamicFont>();

    DynamicFont::initialize_dynamic_fonts();

    ClassDB::register_virtual_class<StyleBox>();
    ClassDB::register_class<StyleBoxEmpty>();
    ClassDB::register_class<StyleBoxTexture>();
    ClassDB::register_class<StyleBoxFlat>();
    ClassDB::register_class<StyleBoxLine>();
    ClassDB::register_class<Theme>();

    ClassDB::register_class<PolygonPathFinder>();
    ClassDB::register_class<BitMap>();
    ClassDB::register_class<Gradient>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_class<AudioStreamPlayer>();
    ClassDB::register_class<AudioStreamPlayer2D>();
#ifndef _3D_DISABLED
    ClassDB::register_class<AudioStreamPlayer3D>();
#endif
    ClassDB::register_virtual_class<VideoStream>();
    ClassDB::register_class<AudioStreamSample>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_virtual_class<Shape2D>();
    ClassDB::register_class<LineShape2D>();
    ClassDB::register_class<SegmentShape2D>();
    ClassDB::register_class<RayShape2D>();
    ClassDB::register_class<CircleShape2D>();
    ClassDB::register_class<RectangleShape2D>();
    ClassDB::register_class<CapsuleShape2D>();
    ClassDB::register_class<ConvexPolygonShape2D>();
    ClassDB::register_class<ConcavePolygonShape2D>();
    ClassDB::register_class<Curve2D>();
    ClassDB::register_class<Path2D>();
    ClassDB::register_class<PathFollow2D>();

    ClassDB::register_class<Navigation2D>();
    ClassDB::register_class<NavigationPolygon>();
    ClassDB::register_class<NavigationPolygonInstance>();
    ClassDB::register_class<NavigationAgent2D>();
    ClassDB::register_class<NavigationObstacle2D>();

    OS::get_singleton()->yield(); //may take time to init

    ClassDB::register_virtual_class<SceneState>();
    ClassDB::register_class<PackedScene>();

    ClassDB::register_class<SceneTree>();
    ClassDB::register_virtual_class<SceneTreeTimer>(); //sorry, you can't create it

#ifndef DISABLE_DEPRECATED
    ClassDB::add_compatibility_class("ImageSkyBox", "PanoramaSky");
    ClassDB::add_compatibility_class("FixedSpatialMaterial", "SpatialMaterial");
    ClassDB::add_compatibility_class("Mesh", "ArrayMesh");

#endif

    OS::get_singleton()->yield(); //may take time to init

    for (int i = 0; i < 20; i++) {
        String idxname = itos(i + 1);
        GLOBAL_DEF(StringName("layer_names/2d_render/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/2d_physics/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/2d_navigation/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/3d_render/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/3d_physics/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/3d_navigation/layer_" + idxname), "");
    }

    for (int i = 20; i < 32; i++) {
        String idxname = itos(i + 1);
        GLOBAL_DEF(StringName("layer_names/2d_physics/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/2d_navigation/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/3d_physics/layer_" + idxname), "");
        GLOBAL_DEF(StringName("layer_names/3d_navigation/layer_" + idxname), "");
    }
}
void initialize_theme() {
    bool default_theme_hidpi = T_GLOBAL_DEF("gui/theme/use_hidpi", false);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/use_hidpi", PropertyInfo(VariantType::BOOL, "gui/theme/use_hidpi", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));
    String theme_path = T_GLOBAL_DEF("gui/theme/custom", String(),true);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/custom", PropertyInfo(VariantType::STRING, "gui/theme/custom", PropertyHint::File, "*.tres,*.res,*.theme", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));
    String font_path = T_GLOBAL_DEF("gui/theme/custom_font", String(),true);
    ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/custom_font", PropertyInfo(VariantType::STRING, "gui/theme/custom_font", PropertyHint::File, "*.tres,*.res,*.font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));

    Ref<Font> font;
    if (!font_path.empty()) {
        font = dynamic_ref_cast<Font>(gResourceManager().load(font_path));
        if (not font) {
            ERR_PRINT("Error loading custom font '" + font_path + "'");
        }
    }

    // Always make the default theme to avoid invalid default font/icon/style in the given theme
    make_default_theme(default_theme_hidpi, font);

    if (!theme_path.empty()) {
        Ref<Theme> theme = dynamic_ref_cast<Theme>(gResourceManager().load(theme_path));
        if (theme) {
            Theme::set_project_default(theme);
            if (font) {
                Theme::set_default_font(font);
            }
        } else {
            ERR_PRINT("Error loading custom theme '" + theme_path + "'");
        }
    }
}

void unregister_scene_types() {

    clear_default_theme();

    gResourceManager().remove_resource_format_loader(resource_loader_dynamic_font);
    resource_loader_dynamic_font.unref();

    gResourceManager().remove_resource_format_loader(resource_loader_texture_layered);
    resource_loader_texture_layered.unref();

    gResourceManager().remove_resource_format_loader(resource_loader_stream_texture);
    resource_loader_stream_texture.unref();

    DynamicFont::finish_dynamic_fonts();

    gResourceManager().remove_resource_format_saver(resource_saver_text);
    resource_saver_text.unref();

    gResourceManager().remove_resource_format_loader(resource_loader_text);
    resource_loader_text.unref();

    gResourceManager().remove_resource_format_saver(resource_saver_shader);
    resource_saver_shader.unref();

    gResourceManager().remove_resource_format_loader(resource_loader_shader);
    resource_loader_shader.unref();

    gResourceManager().remove_resource_format_loader(resource_loader_bmfont);
    resource_loader_bmfont.unref();

    //SpatialMaterial is not initialised when 3D is disabled, so it shouldn't be cleaned up either
#ifndef _3D_DISABLED
    SpatialMaterial::finish_shaders();
#endif // _3D_DISABLED

    ParticlesMaterial::finish_shaders();
    CanvasItemMaterial::finish_shaders();
    SceneStringNames::free();
}
