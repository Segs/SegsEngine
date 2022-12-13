/*************************************************************************/
/*  control_data.h                                                            */
/*************************************************************************/
#pragma once

#include "scene/gui/control.h"
#include "core/hash_map.h"
#include "core/string.h"
#include "core/node_path.h"
#include "core/method_enum_caster.h"
#include "scene/2d/canvas_item.h"
#include "scene/resources/shader.h"

struct ControlData {
    HashMap<StringName, Ref<Texture> > icon_override;
    HashMap<StringName, Ref<Shader> > shader_override;
    HashMap<StringName, Ref<StyleBox> > style_override;
    HashMap<StringName, Ref<Font> > font_override;
    HashMap<StringName, Color> color_override;
    HashMap<StringName, int> constant_override;
    NodePath focus_neighbour[4];
    NodePath focus_next;
    NodePath focus_prev;
    float margin[4] { 0,0,0,0 };
    float anchor[4] { Control::ANCHOR_BEGIN,Control::ANCHOR_BEGIN,Control::ANCHOR_BEGIN,Control::ANCHOR_BEGIN};
    String tooltip;
    StringName theme_type_variation;
    Control * MI = nullptr; //modal item marker, holds pointer to contained object, reset when removed from modal list
    Control * SI = nullptr;
    Control * RI = nullptr; // root item container marker
    CanvasItem *parent_canvas_item = nullptr;

    Control *parent = nullptr;
    Control *theme_owner = nullptr;

    Point2 pos_cache;
    Size2 size_cache;
    Size2 minimum_size_cache;
    Size2 last_minimum_size;

    Control::FocusMode focus_mode = Control::FOCUS_NONE;
    Control::GrowDirection h_grow = Control::GROW_DIRECTION_END;
    Control::GrowDirection v_grow = Control::GROW_DIRECTION_END;

    float rotation = 0;
    Vector2 scale = Vector2(1, 1);
    Vector2 pivot_offset;


    int h_size_flags = Control::SIZE_FILL;
    int v_size_flags = Control::SIZE_FILL;
    float expand = 1;
    Point2 custom_minimum_size;
    Control::MouseFilter mouse_filter = Control::MOUSE_FILTER_STOP;
    GameEntity drag_owner = entt::null;
    GameEntity modal_prev_focus_owner = entt::null;
    uint64_t modal_frame = 0; //frame used to put something as modal
    Ref<Theme> theme;
    Control::CursorShape default_cursor = Control::CURSOR_ARROW;

    uint8_t minimum_size_valid : 1;
    uint8_t updating_last_minimum_size : 1;
    uint8_t pending_resize : 1;
    uint8_t pass_on_modal_close_click : 1;
    uint8_t clip_contents : 1;
    uint8_t block_minimum_size_adjust : 1;
    uint8_t disable_visibility_clip : 1;
    uint8_t modal_exclusive : 1;

    ControlData();
    ~ControlData();
    ControlData(const ControlData &) = delete;
    ControlData &operator=(const ControlData &) = delete;

    ControlData(ControlData &&) noexcept;
    ControlData &operator=(ControlData &&) noexcept;
};

GODOT_EXPORT ControlData &get_control_data(Control *self);
GODOT_EXPORT const ControlData &get_control_data(const Control *self);
