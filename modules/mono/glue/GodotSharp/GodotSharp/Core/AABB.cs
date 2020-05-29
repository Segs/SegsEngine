// file: core/math/aabb.h
// commit: 7ad14e7a3e6f87ddc450f7e34621eb5200808451
// file: core/math/aabb.cpp
// commit: bd282ff43f23fe845f29a3e25c8efc01bd65ffb0
// file: core/variant_call.cpp
// commit: 5ad9be4c24e9d7dc5672fdc42cea896622fe5685
using System;
using System.Runtime.InteropServices;

namespace Godot
{
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct AABB : IEquatable<AABB>
    {
        private Vector3 _position;
        private Vector3 _size;

        public Vector3 Position
        {
            get { return _position; }
            set { _position = value; }
        }

        public Vector3 Size
        {
            get { return _size; }
            set { _size = value; }
        }

        public Vector3 End
        {
            get { return _position + _size; }
            set { _size = value - _position; }
        }

        public bool Encloses(AABB with)
        {
            Vector3 src_min = _position;
            Vector3 src_max = _position + _size;
            Vector3 dst_min = with._position;
            Vector3 dst_max = with._position + with._size;

            return src_min.x <= dst_min.x &&
                   src_max.x > dst_max.x &&
                   src_min.y <= dst_min.y &&
                   src_max.y > dst_max.y &&
                   src_min.z <= dst_min.z &&
                   src_max.z > dst_max.z;
        }

        public AABB Expand(Vector3 point)
        {
            Vector3 begin = _position;
            Vector3 end = _position + _size;

            if (point.x < begin.x)
                begin.x = point.x;
            if (point.y < begin.y)
                begin.y = point.y;
            if (point.z < begin.z)
                begin.z = point.z;

            if (point.x > end.x)
                end.x = point.x;
            if (point.y > end.y)
                end.y = point.y;
            if (point.z > end.z)
                end.z = point.z;

            return new AABB(begin, end - begin);
        }

        public float GetArea()
        {
            return _size.x * _size.y * _size.z;
        }

        public Vector3 GetEndpoint(int idx)
        {
            switch (idx)
            {
                case 0:
                    return new Vector3(_position.x, _position.y, _position.z);
                case 1:
                    return new Vector3(_position.x, _position.y, _position.z + _size.z);
                case 2:
                    return new Vector3(_position.x, _position.y + _size.y, _position.z);
                case 3:
                    return new Vector3(_position.x, _position.y + _size.y, _position.z + _size.z);
                case 4:
                    return new Vector3(_position.x + _size.x, _position.y, _position.z);
                case 5:
                    return new Vector3(_position.x + _size.x, _position.y, _position.z + _size.z);
                case 6:
                    return new Vector3(_position.x + _size.x, _position.y + _size.y, _position.z);
                case 7:
                    return new Vector3(_position.x + _size.x, _position.y + _size.y, _position.z + _size.z);
                default:
                    throw new ArgumentOutOfRangeException(nameof(idx), String.Format("Index is {0}, but a value from 0 to 7 is expected.", idx));
            }
        }

        public Vector3 GetLongestAxis()
        {
            var axis = new Vector3(1f, 0f, 0f);
            float max_size = _size.x;

            if (_size.y > max_size)
            {
                axis = new Vector3(0f, 1f, 0f);
                max_size = _size.y;
            }

            if (_size.z > max_size)
            {
                axis = new Vector3(0f, 0f, 1f);
            }

            return axis;
        }

        public Vector3.Axis GetLongestAxisIndex()
        {
            var axis = Vector3.Axis.X;
            float max_size = _size.x;

            if (_size.y > max_size)
            {
                axis = Vector3.Axis.Y;
                max_size = _size.y;
            }

            if (_size.z > max_size)
            {
                axis = Vector3.Axis.Z;
            }

            return axis;
        }

        public float GetLongestAxisSize()
        {
            float max_size = _size.x;

            if (_size.y > max_size)
                max_size = _size.y;

            if (_size.z > max_size)
                max_size = _size.z;

            return max_size;
        }

        public Vector3 GetShortestAxis()
        {
            var axis = new Vector3(1f, 0f, 0f);
            float max_size = _size.x;

            if (_size.y < max_size)
            {
                axis = new Vector3(0f, 1f, 0f);
                max_size = _size.y;
            }

            if (_size.z < max_size)
            {
                axis = new Vector3(0f, 0f, 1f);
            }

            return axis;
        }

        public Vector3.Axis GetShortestAxisIndex()
        {
            var axis = Vector3.Axis.X;
            float max_size = _size.x;

            if (_size.y < max_size)
            {
                axis = Vector3.Axis.Y;
                max_size = _size.y;
            }

            if (_size.z < max_size)
            {
                axis = Vector3.Axis.Z;
            }

            return axis;
        }

        public float GetShortestAxisSize()
        {
            float max_size = _size.x;

            if (_size.y < max_size)
                max_size = _size.y;

            if (_size.z < max_size)
                max_size = _size.z;

            return max_size;
        }

        public Vector3 GetSupport(Vector3 dir)
        {
            Vector3 half_extents = _size * 0.5f;
            Vector3 ofs = _position + half_extents;

            return ofs + new Vector3(
                dir.x > 0f ? -half_extents.x : half_extents.x,
                dir.y > 0f ? -half_extents.y : half_extents.y,
                dir.z > 0f ? -half_extents.z : half_extents.z);
        }

        public AABB Grow(float by)
        {
            var res = this;

            res._position.x -= by;
            res._position.y -= by;
            res._position.z -= by;
            res._size.x += 2.0f * by;
            res._size.y += 2.0f * by;
            res._size.z += 2.0f * by;

            return res;
        }

        public bool HasNoArea()
        {
            return _size.x <= 0f || _size.y <= 0f || _size.z <= 0f;
        }

        public bool HasNoSurface()
        {
            return _size.x <= 0f && _size.y <= 0f && _size.z <= 0f;
        }

        public bool HasPoint(Vector3 point)
        {
            if (point.x < _position.x)
                return false;
            if (point.y < _position.y)
                return false;
            if (point.z < _position.z)
                return false;
            if (point.x > _position.x + _size.x)
                return false;
            if (point.y > _position.y + _size.y)
                return false;
            if (point.z > _position.z + _size.z)
                return false;

            return true;
        }

        public AABB Intersection(AABB with)
        {
            Vector3 src_min = _position;
            Vector3 src_max = _position + _size;
            Vector3 dst_min = with._position;
            Vector3 dst_max = with._position + with._size;

            Vector3 min, max;

            if (src_min.x > dst_max.x || src_max.x < dst_min.x)
            {
                return new AABB();
            }

            min.x = src_min.x > dst_min.x ? src_min.x : dst_min.x;
            max.x = src_max.x < dst_max.x ? src_max.x : dst_max.x;

            if (src_min.y > dst_max.y || src_max.y < dst_min.y)
            {
                return new AABB();
            }

            min.y = src_min.y > dst_min.y ? src_min.y : dst_min.y;
            max.y = src_max.y < dst_max.y ? src_max.y : dst_max.y;

            if (src_min.z > dst_max.z || src_max.z < dst_min.z)
            {
                return new AABB();
            }

            min.z = src_min.z > dst_min.z ? src_min.z : dst_min.z;
            max.z = src_max.z < dst_max.z ? src_max.z : dst_max.z;

            return new AABB(min, max - min);
        }

        public bool Intersects(AABB with)
        {
            if (_position.x >= with._position.x + with._size.x)
                return false;
            if (_position.x + _size.x <= with._position.x)
                return false;
            if (_position.y >= with._position.y + with._size.y)
                return false;
            if (_position.y + _size.y <= with._position.y)
                return false;
            if (_position.z >= with._position.z + with._size.z)
                return false;
            if (_position.z + _size.z <= with._position.z)
                return false;

            return true;
        }

        public bool IntersectsPlane(Plane plane)
        {
            Vector3[] points =
            {
                new Vector3(_position.x, _position.y, _position.z),
                new Vector3(_position.x, _position.y, _position.z + _size.z),
                new Vector3(_position.x, _position.y + _size.y, _position.z),
                new Vector3(_position.x, _position.y + _size.y, _position.z + _size.z),
                new Vector3(_position.x + _size.x, _position.y, _position.z),
                new Vector3(_position.x + _size.x, _position.y, _position.z + _size.z),
                new Vector3(_position.x + _size.x, _position.y + _size.y, _position.z),
                new Vector3(_position.x + _size.x, _position.y + _size.y, _position.z + _size.z)
            };

            bool over = false;
            bool under = false;

            for (int i = 0; i < 8; i++)
            {
                if (plane.DistanceTo(points[i]) > 0)
                    over = true;
                else
                    under = true;
            }

            return under && over;
        }

        public bool IntersectsSegment(Vector3 from, Vector3 to)
        {
            float min = 0f;
            float max = 1f;

            for (int i = 0; i < 3; i++)
            {
                float segFrom = from[i];
                float segTo = to[i];
                float boxBegin = _position[i];
                float boxEnd = boxBegin + _size[i];
                float cmin, cmax;

                if (segFrom < segTo)
                {
                    if (segFrom > boxEnd || segTo < boxBegin)
                        return false;

                    float length = segTo - segFrom;
                    cmin = segFrom < boxBegin ? (boxBegin - segFrom) / length : 0f;
                    cmax = segTo > boxEnd ? (boxEnd - segFrom) / length : 1f;
                }
                else
                {
                    if (segTo > boxEnd || segFrom < boxBegin)
                        return false;

                    float length = segTo - segFrom;
                    cmin = segFrom > boxEnd ? (boxEnd - segFrom) / length : 0f;
                    cmax = segTo < boxBegin ? (boxBegin - segFrom) / length : 1f;
                }

                if (cmin > min)
                {
                    min = cmin;
                }

                if (cmax < max)
                    max = cmax;
                if (max < min)
                    return false;
            }

            return true;
        }

        public AABB Merge(AABB with)
        {
            Vector3 beg1 = _position;
            Vector3 beg2 = with._position;
            var end1 = new Vector3(_size.x, _size.y, _size.z) + beg1;
            var end2 = new Vector3(with._size.x, with._size.y, with._size.z) + beg2;

            var min = new Vector3(
                              beg1.x < beg2.x ? beg1.x : beg2.x,
                              beg1.y < beg2.y ? beg1.y : beg2.y,
                              beg1.z < beg2.z ? beg1.z : beg2.z
                          );

            var max = new Vector3(
                              end1.x > end2.x ? end1.x : end2.x,
                              end1.y > end2.y ? end1.y : end2.y,
                              end1.z > end2.z ? end1.z : end2.z
                          );

            return new AABB(min, max - min);
        }

        // Constructors
        public AABB(Vector3 position, Vector3 size)
        {
            _position = position;
            _size = size;
        }
        public AABB(Vector3 position, float width, float height, float depth)
        {
            _position = position;
            _size = new Vector3(width, height, depth);
        }
        public AABB(float x, float y, float z, Vector3 size)
        {
            _position = new Vector3(x, y, z);
            _size = size;
        }
        public AABB(float x, float y, float z, float width, float height, float depth)
        {
            _position = new Vector3(x, y, z);
            _size = new Vector3(width, height, depth);
        }

        public static bool operator ==(AABB left, AABB right)
        {
            return left.Equals(right);
        }

        public static bool operator !=(AABB left, AABB right)
        {
            return !left.Equals(right);
        }

        public override bool Equals(object obj)
        {
            if (obj is AABB)
            {
                return Equals((AABB)obj);
            }

            return false;
        }

        public bool Equals(AABB other)
        {
            return _position == other._position && _size == other._size;
        }

        public bool IsEqualApprox(AABB other)
        {
            return _position.IsEqualApprox(other._position) && _size.IsEqualApprox(other._size);
        }

        public override int GetHashCode()
        {
            return _position.GetHashCode() ^ _size.GetHashCode();
        }

        public override string ToString()
        {
            return String.Format("{0} - {1}", new object[]
                {
                    _position.ToString(),
                    _size.ToString()
                });
        }

        public string ToString(string format)
        {
            return String.Format("{0} - {1}", new object[]
                {
                    _position.ToString(format),
                    _size.ToString(format)
                });
        }
    }
}
