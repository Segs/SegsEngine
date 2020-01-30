/*************************************************************************/
/*  collision_polygon.h                                                  */
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

#pragma once

#include "scene/3d/spatial.h"
#include "scene/resources/shape.h"
#include "core/math/vector2.h"

class CollisionObject;
class CollisionPolygon : public Spatial {

	GDCLASS(CollisionPolygon,Spatial)

protected:
    AABB aabb;
    PODVector<Vector2> polygon;
	float depth;

	uint32_t owner_id;
	CollisionObject *parent;

	bool disabled;

	void _build_polygon();

	void _update_in_shape_owner(bool p_xform_only = false);

	bool _is_editable_3d_polygon() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_depth(float p_depth);
	float get_depth() const;

    void set_polygon(const PODVector<Vector2> &p_polygon);
    const PODVector<Vector2> &get_polygon() const;

	void set_disabled(bool p_disabled);
	bool is_disabled() const;

	virtual AABB get_item_rect() const;

    StringName get_configuration_warning() const override;

	CollisionPolygon();
};
