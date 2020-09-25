/*************************************************************************/
/*  shape_bullet.cpp                                                     */
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

#include "shape_bullet.h"

#include "btRayShape.h"
#include "bullet_physics_server.h"
#include "bullet_types_converter.h"
#include "bullet_utilities.h"
#include "core/project_settings.h"
#include "core/image.h"
#include "shape_owner_bullet.h"

#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>
#include <BulletCollision/CollisionShapes/btConvexPointCloudShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <btBulletCollisionCommon.h>



/**
    @author AndreaCatania
*/

ShapeBullet::ShapeBullet() :
        margin(0.04f) {}

ShapeBullet::~ShapeBullet() {}

btCollisionShape *ShapeBullet::create_bt_shape(const Vector3 &p_implicit_scale, real_t p_extra_edge) {
    btVector3 s;
    G_TO_B(p_implicit_scale, s);
    return create_bt_shape(s, p_extra_edge);
}

btCollisionShape *ShapeBullet::prepare(btCollisionShape *p_btShape) const {
    p_btShape->setUserPointer(const_cast<ShapeBullet *>(this));
    p_btShape->setMargin(margin);
    return p_btShape;
}

void ShapeBullet::notifyShapeChanged() {
    for (eastl::pair<ShapeOwnerBullet *,int> E : owners) {
        ShapeOwnerBullet *owner = static_cast<ShapeOwnerBullet *>(E.first);
        owner->shape_changed(owner->find_shape(this));
    }
}

void ShapeBullet::add_owner(ShapeOwnerBullet *p_owner) {
    owners[p_owner]++; // add new owner
}

void ShapeBullet::remove_owner(ShapeOwnerBullet *p_owner, bool p_permanentlyFromThisBody) {
    auto E = owners.find(p_owner);
    if (E==owners.end())
        return;
    E->second--;
    if (p_permanentlyFromThisBody || 0 >= E->second) {
        owners.erase(E);
    }
}

bool ShapeBullet::is_owner(ShapeOwnerBullet *p_owner) const {

    return owners.contains(p_owner);
}



void ShapeBullet::set_margin(real_t p_margin) {
    margin = p_margin;
    notifyShapeChanged();
}

real_t ShapeBullet::get_margin() const {
    return margin;
}

btEmptyShape *ShapeBullet::create_shape_empty() {
    return bulletnew(btEmptyShape);
}

btStaticPlaneShape *ShapeBullet::create_shape_plane(const btVector3 &planeNormal, float planeConstant) {
    return bulletnew(btStaticPlaneShape(planeNormal, planeConstant));
}

btSphereShape *ShapeBullet::create_shape_sphere(float radius) {
    return bulletnew(btSphereShape(radius));
}

btBoxShape *ShapeBullet::create_shape_box(const btVector3 &boxHalfExtents) {
    return bulletnew(btBoxShape(boxHalfExtents));
}

btCapsuleShapeZ *ShapeBullet::create_shape_capsule(float radius, float height) {
    return bulletnew(btCapsuleShapeZ(radius, height));
}

btCylinderShape *ShapeBullet::create_shape_cylinder(float radius, float height) {
    return bulletnew(btCylinderShape(btVector3(radius, height / 2.0, radius)));
}

btConvexPointCloudShape *ShapeBullet::create_shape_convex(btAlignedObjectArray<btVector3> &p_vertices, const btVector3 &p_local_scaling) {
    return bulletnew(btConvexPointCloudShape(&p_vertices[0], p_vertices.size(), p_local_scaling));
}

btScaledBvhTriangleMeshShape *ShapeBullet::create_shape_concave(btBvhTriangleMeshShape *p_mesh_shape, const btVector3 &p_local_scaling) {
    if (p_mesh_shape) {
        return bulletnew(btScaledBvhTriangleMeshShape(p_mesh_shape, p_local_scaling));
    } else {
        return nullptr;
    }
}

btHeightfieldTerrainShape *ShapeBullet::create_shape_height_field(PoolVector<real_t> &p_heights, int p_width, int p_depth, real_t p_min_height, real_t p_max_height) {
    const btScalar ignoredHeightScale(1);
    const int YAxis = 1; // 0=X, 1=Y, 2=Z
    const bool flipQuadEdges = false;
    const void *heightsPtr = p_heights.read().ptr();

    btHeightfieldTerrainShape *heightfield = bulletnew(btHeightfieldTerrainShape(p_width, p_depth, heightsPtr, ignoredHeightScale, p_min_height, p_max_height, YAxis, PHY_FLOAT, flipQuadEdges));

    // The shape can be created without params when you do PhysicsServer3D.shape_create(PhysicsServer3D.SHAPE_HEIGHTMAP)
    if (heightsPtr)
        heightfield->buildAccelerator(16);

    return heightfield;
}

btRayShape *ShapeBullet::create_shape_ray(real_t p_length, bool p_slips_on_slope) {
    btRayShape *r(bulletnew(btRayShape(p_length)));
    r->setSlipsOnSlope(p_slips_on_slope);
    return r;
}

/* PLANE */

PlaneShapeBullet::PlaneShapeBullet() :
        ShapeBullet() {}

void PlaneShapeBullet::set_data(const Variant &p_data) {
    setup(p_data.as<Plane>());
}

Variant PlaneShapeBullet::get_data() const {
    return plane;
}

PhysicsServer3D::ShapeType PlaneShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_PLANE;
}

void PlaneShapeBullet::setup(const Plane &p_plane) {
    plane = p_plane;
    notifyShapeChanged();
}

btCollisionShape *PlaneShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    btVector3 btPlaneNormal;
    G_TO_B(plane.normal, btPlaneNormal);
    return prepare(PlaneShapeBullet::create_shape_plane(btPlaneNormal, plane.d));
}

/* Sphere */

SphereShapeBullet::SphereShapeBullet() :
        ShapeBullet() {}

void SphereShapeBullet::set_data(const Variant &p_data) {
    setup(p_data.as<float>());
}

Variant SphereShapeBullet::get_data() const {
    return radius;
}

PhysicsServer3D::ShapeType SphereShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_SPHERE;
}

void SphereShapeBullet::setup(real_t p_radius) {
    radius = p_radius;
    notifyShapeChanged();
}

btCollisionShape *SphereShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    return prepare(ShapeBullet::create_shape_sphere(radius * p_implicit_scale[0] + p_extra_edge));
}

/* Box */
BoxShapeBullet::BoxShapeBullet() :
        ShapeBullet() {}

void BoxShapeBullet::set_data(const Variant &p_data) {
    setup(p_data.as<Vector3>());
}

Variant BoxShapeBullet::get_data() const {
    Vector3 g_half_extents;
    B_TO_G(half_extents, g_half_extents);
    return g_half_extents;
}

PhysicsServer3D::ShapeType BoxShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_BOX;
}

void BoxShapeBullet::setup(const Vector3 &p_half_extents) {
    G_TO_B(p_half_extents, half_extents);
    notifyShapeChanged();
}

btCollisionShape *BoxShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    return prepare(ShapeBullet::create_shape_box((half_extents * p_implicit_scale) + btVector3(p_extra_edge, p_extra_edge, p_extra_edge)));
}

/* Capsule */

CapsuleShapeBullet::CapsuleShapeBullet() :
        ShapeBullet() {}

void CapsuleShapeBullet::set_data(const Variant &p_data) {
    Dictionary d = p_data.as<Dictionary>();
    ERR_FAIL_COND(!d.has("radius"));
    ERR_FAIL_COND(!d.has("height"));
    setup(d["height"].as<float>(), d["radius"].as<float>());
}

Variant CapsuleShapeBullet::get_data() const {
    Dictionary d;
    d["radius"] = radius;
    d["height"] = height;
    return d;
}

PhysicsServer3D::ShapeType CapsuleShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_CAPSULE;
}

void CapsuleShapeBullet::setup(real_t p_height, real_t p_radius) {
    radius = p_radius;
    height = p_height;
    notifyShapeChanged();
}

btCollisionShape *CapsuleShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    return prepare(ShapeBullet::create_shape_capsule(radius * p_implicit_scale[0] + p_extra_edge, height * p_implicit_scale[1] + p_extra_edge));
}

/* Cylinder */

CylinderShapeBullet::CylinderShapeBullet() :
        ShapeBullet() {}

void CylinderShapeBullet::set_data(const Variant &p_data) {
    Dictionary d = p_data.as<Dictionary>();
    ERR_FAIL_COND(!d.has("radius"));
    ERR_FAIL_COND(!d.has("height"));
    setup(d["height"].as<float>(), d["radius"].as<float>());
}

Variant CylinderShapeBullet::get_data() const {
    Dictionary d;
    d["radius"] = radius;
    d["height"] = height;
    return d;
}

PhysicsServer3D::ShapeType CylinderShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_CYLINDER;
}

void CylinderShapeBullet::setup(real_t p_height, real_t p_radius) {
    radius = p_radius;
    height = p_height;
    notifyShapeChanged();
}

btCollisionShape *CylinderShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_margin) {
    return prepare(ShapeBullet::create_shape_cylinder(radius * p_implicit_scale[0] + p_margin, height * p_implicit_scale[1] + p_margin));
}

/* Convex polygon */

ConvexPolygonShapeBullet::ConvexPolygonShapeBullet() :
        ShapeBullet() {}

void ConvexPolygonShapeBullet::set_data(const Variant &p_data) {
    setup(p_data.as<Vector<Vector3>>());
}

void ConvexPolygonShapeBullet::get_vertices(Vector<Vector3> &out_vertices) {
    const int n_of_vertices = vertices.size();
    out_vertices.resize(n_of_vertices);
    for (int i = n_of_vertices - 1; 0 <= i; --i) {
        B_TO_G(vertices[i], out_vertices[i]);
    }
}

Variant ConvexPolygonShapeBullet::get_data() const {
    ConvexPolygonShapeBullet *variable_self = const_cast<ConvexPolygonShapeBullet *>(this);
    Vector<Vector3> out_vertices;
    variable_self->get_vertices(out_vertices);
    return Variant::from(out_vertices);
}

PhysicsServer3D::ShapeType ConvexPolygonShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_CONVEX_POLYGON;
}

void ConvexPolygonShapeBullet::setup(const Vector<Vector3> &p_vertices) {
    // Make a copy of vertices
    const int n_of_vertices = p_vertices.size();
    vertices.resize(n_of_vertices);
    for (int i = n_of_vertices - 1; 0 <= i; --i) {
        G_TO_B(p_vertices[i], vertices[i]);
    }
    notifyShapeChanged();
}

btCollisionShape *ConvexPolygonShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    if (!vertices.size())
        // This is necessary since 0 vertices
        return prepare(ShapeBullet::create_shape_empty());
    btCollisionShape *cs(ShapeBullet::create_shape_convex(vertices));
    cs->setLocalScaling(p_implicit_scale);
    prepare(cs);
    return cs;
}

/* Concave polygon */

ConcavePolygonShapeBullet::ConcavePolygonShapeBullet() :
        ShapeBullet(),
        meshShape(nullptr) {}

ConcavePolygonShapeBullet::~ConcavePolygonShapeBullet() {
    if (meshShape) {
        delete meshShape->getMeshInterface();
        delete meshShape->getTriangleInfoMap();
        bulletdelete(meshShape);
    }
    faces = PoolVector<Vector3>();
}

void ConcavePolygonShapeBullet::set_data(const Variant &p_data) {
    setup(p_data.as<PoolVector<Vector3>>());
}

Variant ConcavePolygonShapeBullet::get_data() const {
    return faces;
}

PhysicsServer3D::ShapeType ConcavePolygonShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_CONCAVE_POLYGON;
}

void ConcavePolygonShapeBullet::setup(const PoolVector<Vector3>& p_faces) {
    faces = p_faces;
    if (meshShape) {
        /// Clear previous created shape
        delete meshShape->getMeshInterface();
        delete meshShape->getTriangleInfoMap();
        bulletdelete(meshShape);
    }
    int src_face_count = faces.size();
    if (0 < src_face_count) {

        // It counts the faces and assert the array contains the correct number of vertices.
        ERR_FAIL_COND(src_face_count % 3);

        btTriangleMesh *shapeInterface = bulletnew(btTriangleMesh);
        src_face_count /= 3;
        PoolVector<Vector3>::Read r = p_faces.read();
        const Vector3 *facesr = r.ptr();

        btVector3 supVec_0;
        btVector3 supVec_1;
        btVector3 supVec_2;
        for (int i = 0; i < src_face_count; ++i) {
            G_TO_B(facesr[i * 3 + 0], supVec_0);
            G_TO_B(facesr[i * 3 + 1], supVec_1);
            G_TO_B(facesr[i * 3 + 2], supVec_2);

            // Inverted from standard godot otherwise btGenerateInternalEdgeInfo generates wrong edge info
            shapeInterface->addTriangle(supVec_2, supVec_1, supVec_0);
        }

        const bool useQuantizedAabbCompression = true;

        meshShape = bulletnew(btBvhTriangleMeshShape(shapeInterface, useQuantizedAabbCompression));

        if (T_GLOBAL_DEF("physics/3d/smooth_trimesh_collision", false)) {
            btTriangleInfoMap *triangleInfoMap = new btTriangleInfoMap();
            btGenerateInternalEdgeInfo(meshShape, triangleInfoMap);
        }
    } else {
        meshShape = nullptr;
        ERR_PRINT("The faces count are 0, the mesh shape cannot be created");
    }
    notifyShapeChanged();
}

btCollisionShape *ConcavePolygonShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    btCollisionShape *cs = ShapeBullet::create_shape_concave(meshShape);
    if (!cs)
        // This is necessary since if 0 faces the creation of concave return NULL
        cs = ShapeBullet::create_shape_empty();
    cs->setLocalScaling(p_implicit_scale);
    prepare(cs);
    cs->setMargin(0);
    return cs;
}

/* Height map shape */

HeightMapShapeBullet::HeightMapShapeBullet() :
        ShapeBullet() {}

void HeightMapShapeBullet::set_data(const Variant &p_data) {
    ERR_FAIL_COND(p_data.get_type() != VariantType::DICTIONARY);
    Dictionary d = p_data.as<Dictionary>();
    ERR_FAIL_COND(!d.has("width"));
    ERR_FAIL_COND(!d.has("depth"));
    ERR_FAIL_COND(!d.has("heights"));

    real_t l_min_height = 0.0;
    real_t l_max_height = 0.0;

    // If specified, min and max height will be used as precomputed values
    if (d.has("min_height"))
        l_min_height = d["min_height"].as<float>();
    if (d.has("max_height"))
        l_max_height = d["max_height"].as<float>();

    ERR_FAIL_COND(l_min_height > l_max_height);

    int l_width = d["width"].as<int>();
    int l_depth = d["depth"].as<int>();

    ERR_FAIL_COND_MSG(l_width < 2, "Map width must be at least 2.");
    ERR_FAIL_COND_MSG(l_depth < 2, "Map depth must be at least 2.");


    // TODO This code will need adjustments if real_t is set to `double`,
    // because that precision is unnecessary for a heightmap and Bullet doesn't support it...

    PoolVector<real_t> l_heights;
    Variant l_heights_v = d["heights"];

    if (l_heights_v.get_type() == VariantType::POOL_REAL_ARRAY) {
        // Ready-to-use heights can be passed

        l_heights = l_heights_v.as<PoolVector<real_t>>();

    } else if (l_heights_v.get_type() == VariantType::OBJECT) {
        // If an image is passed, we have to convert it to a format Bullet supports.
        // this would be expensive to do with a script, so it's nice to have it here.

        Ref<Image> l_image = refFromRefPtr<Image>(l_heights_v.as<RefPtr>());
        ERR_FAIL_COND(not l_image);

        // Float is the only common format between Godot and Bullet that can be used for decent collision.
        // (Int16 would be nice too but we still don't have it)
        // We could convert here automatically but it's better to not be intrusive and let the caller do it if necessary.
        ERR_FAIL_COND(l_image->get_format() != Image::FORMAT_RF);

        PoolByteArray im_data = l_image->get_data();

        l_heights.resize(l_image->get_width() * l_image->get_height());

        PoolRealArray::Write w = l_heights.write();
        PoolByteArray::Read r = im_data.read();
        float *rp = (float *)r.ptr();
        // At this point, `rp` could be used directly for Bullet, but I don't know how safe it would be.

        for (int i = 0; i < l_heights.size(); ++i) {
            w[i] = rp[i];
        }

    } else {
        ERR_FAIL_MSG("Expected PoolRealArray or float Image.");
    }

    ERR_FAIL_COND(l_width <= 0);
    ERR_FAIL_COND(l_depth <= 0);
    ERR_FAIL_COND(l_heights.size() != (l_width * l_depth));

    // Compute min and max heights if not specified.
    if (!d.has("min_height") && !d.has("max_height")) {

        PoolVector<real_t>::Read r = l_heights.read();
        int heights_size = l_heights.size();

        for (int i = 0; i < heights_size; ++i) {
            real_t h = r[i];

            if (h < l_min_height) {
                l_min_height = h;
            } else if (h > l_max_height) {
                l_max_height = h;
            }
        }
    }

    setup(l_heights, l_width, l_depth, l_min_height, l_max_height);
}

Variant HeightMapShapeBullet::get_data() const {
    ERR_FAIL_V(Variant());
}

PhysicsServer3D::ShapeType HeightMapShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_HEIGHTMAP;
}

void HeightMapShapeBullet::setup(PoolVector<real_t> &p_heights, int p_width, int p_depth, real_t p_min_height, real_t p_max_height) {
    // TODO cell size must be tweaked using localScaling, which is a shared property for all Bullet shapes

    // If this array is resized outside of here, it should be preserved due to CoW
    heights = p_heights;

    width = p_width;
    depth = p_depth;
    min_height = p_min_height;
    max_height = p_max_height;
    notifyShapeChanged();
}

btCollisionShape *HeightMapShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    btCollisionShape *cs(ShapeBullet::create_shape_height_field(heights, width, depth, min_height, max_height));
    cs->setLocalScaling(p_implicit_scale);
    prepare(cs);
    return cs;
}

/* Ray shape */
RayShapeBullet::RayShapeBullet() :
        ShapeBullet(),
        length(1),
        slips_on_slope(false) {}

void RayShapeBullet::set_data(const Variant &p_data) {

    Dictionary d = p_data.as<Dictionary>();
    setup(d["length"].as<float>(), d["slips_on_slope"].as<bool>());
}

Variant RayShapeBullet::get_data() const {

    Dictionary d;
    d["length"] = length;
    d["slips_on_slope"] = slips_on_slope;
    return d;
}

PhysicsServer3D::ShapeType RayShapeBullet::get_type() const {
    return PhysicsServer3D::SHAPE_RAY;
}

void RayShapeBullet::setup(real_t p_length, bool p_slips_on_slope) {
    length = p_length;
    slips_on_slope = p_slips_on_slope;
    notifyShapeChanged();
}

btCollisionShape *RayShapeBullet::create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge) {
    return prepare(ShapeBullet::create_shape_ray(length * p_implicit_scale[1] + p_extra_edge, slips_on_slope));
}
