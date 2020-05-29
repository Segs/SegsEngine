using System;

namespace Godot
{
    public static partial class Mathf
    {
        // Define constants with Decimal precision and cast down to double or float.

        public const float Tau = (float) 6.2831853071795864769252867666M; // 6.2831855f and 6.28318530717959
        public const float Pi = (float) 3.1415926535897932384626433833M; // 3.1415927f and 3.14159265358979
        public const float Inf = float.PositiveInfinity;
        public const float NaN = float.NaN;

        private const float Deg2RadConst = (float) 0.0174532925199432957692369077M; // 0.0174532924f and 0.0174532925199433
        private const float Rad2DegConst = (float) 57.295779513082320876798154814M; // 57.29578f and 57.2957795130823

        public static int Abs(int s)
        {
            return Math.Abs(s);
        }

        public static float Abs(float s)
        {
            return Math.Abs(s);
        }

        public static float Acos(float s)
        {
            return (float)Math.Acos(s);
        }

        public static float Asin(float s)
        {
            return (float)Math.Asin(s);
        }

        public static float Atan(float s)
        {
            return (float)Math.Atan(s);
        }

        public static float Atan2(float y, float x)
        {
            return (float)Math.Atan2(y, x);
        }

        public static Vector2 Cartesian2Polar(float x, float y)
        {
            return new Vector2(Sqrt(x * x + y * y), Atan2(y, x));
        }

        public static float Ceil(float s)
        {
            return (float)Math.Ceiling(s);
        }

        public static int Clamp(int value, int min, int max)
        {
            return value < min ? min : value > max ? max : value;
        }

        public static float Clamp(float value, float min, float max)
        {
            return value < min ? min : value > max ? max : value;
        }

        public static float Cos(float s)
        {
            return (float)Math.Cos(s);
        }

        public static float Cosh(float s)
        {
            return (float)Math.Cosh(s);
        }

        public static float Deg2Rad(float deg)
        {
            return deg * Deg2RadConst;
        }

        public static float Ease(float s, float curve)
        {
            if (s < 0f)
            {
                s = 0f;
            }
            else if (s > 1.0f)
            {
                s = 1.0f;
            }

            if (curve > 0f)
            {
                if (curve < 1.0f)
                {
                    return 1.0f - Pow(1.0f - s, 1.0f / curve);
                }

                return Pow(s, curve);
            }

            if (curve < 0f)
            {
                if (s < 0.5f)
                {
                    return Pow(s * 2.0f, -curve) * 0.5f;
                }

                return (1.0f - Pow(1.0f - (s - 0.5f) * 2.0f, -curve)) * 0.5f + 0.5f;
            }

            return 0f;
        }

        public static float Exp(float s)
        {
            return (float)Math.Exp(s);
        }

        public static float Floor(float s)
        {
            return (float)Math.Floor(s);
        }

        public static float InverseLerp(float from, float to, float weight)
        {
           return (weight - from) / (to - from);
        }

        public static bool IsEqualApprox(float a, float b)
        {
            // Check for exact equality first, required to handle "infinity" values.
            if (a == b)
            {
                return true;
            }
            // Then check for approximate equality.
            float tolerance = Epsilon * Abs(a);
            if (tolerance < Epsilon)
            {
                tolerance = Epsilon;
            }
            return Abs(a - b) < tolerance;
        }

        public static bool IsInf(float s)
        {
           return float.IsInfinity(s);
        }

        public static bool IsNaN(float s)
        {
           return float.IsNaN(s);
        }

        public static bool IsZeroApprox(float s)
        {
            return Abs(s) < Epsilon;
        }

        public static float Lerp(float from, float to, float weight)
        {
            return from + (to - from) * weight;
        }

        public static float LerpAngle(float from, float to, float weight)
        {
            float difference = (to - from) % Mathf.Tau;
            float distance = ((2 * difference) % Mathf.Tau) - difference;
            return from + distance * weight;
        }

        public static float Log(float s)
        {
            return (float)Math.Log(s);
        }

        public static int Max(int a, int b)
        {
            return a > b ? a : b;
        }

        public static float Max(float a, float b)
        {
            return a > b ? a : b;
        }

        public static int Min(int a, int b)
        {
            return a < b ? a : b;
        }

        public static float Min(float a, float b)
        {
            return a < b ? a : b;
        }

        public static float MoveToward(float from, float to, float delta)
        {
            return Abs(to - from) <= delta ? to : from + Sign(to - from) * delta;
        }

        public static int NearestPo2(int value)
        {
            value--;
            value |= value >> 1;
            value |= value >> 2;
            value |= value >> 4;
            value |= value >> 8;
            value |= value >> 16;
            value++;
            return value;
        }

        public static Vector2 Polar2Cartesian(float r, float th)
        {
            return new Vector2(r * Cos(th), r * Sin(th));
        }

        /// <summary>
        /// Performs a canonical Modulus operation, where the output is on the range [0, b).
        /// </summary>
        public static int PosMod(int a, int b)
        {
            int c = a % b;
            if ((c < 0 && b > 0) || (c > 0 && b < 0))
            {
                c += b;
            }
            return c;
        }

        /// <summary>
        /// Performs a canonical Modulus operation, where the output is on the range [0, b).
        /// </summary>
        public static float PosMod(float a, float b)
        {
            float c = a % b;
            if ((c < 0 && b > 0) || (c > 0 && b < 0))
            {
                c += b;
            }
            return c;
        }

        public static float Pow(float x, float y)
        {
            return (float)Math.Pow(x, y);
        }

        public static float Rad2Deg(float rad)
        {
            return rad * Rad2DegConst;
        }

        public static float Round(float s)
        {
            return (float)Math.Round(s);
        }

        public static int Sign(int s)
        {
            if (s == 0) return 0;
            return s < 0 ? -1 : 1;
        }

        public static int Sign(float s)
        {
            if (s == 0) return 0;
            return s < 0 ? -1 : 1;
        }

        public static float Sin(float s)
        {
            return (float)Math.Sin(s);
        }

        public static float Sinh(float s)
        {
            return (float)Math.Sinh(s);
        }

        public static float SmoothStep(float from, float to, float weight)
        {
            if (IsEqualApprox(from, to))
            {
                return from;
            }
            float x = Clamp((weight - from) / (to - from), (float)0.0, (float)1.0);
            return x * x * (3 - 2 * x);
        }

        public static float Sqrt(float s)
        {
            return (float)Math.Sqrt(s);
        }

        public static int StepDecimals(float step)
        {
            double[] sd = new double[] {
                0.9999,
                0.09999,
                0.009999,
                0.0009999,
                0.00009999,
                0.000009999,
                0.0000009999,
                0.00000009999,
                0.000000009999,
            };
            double abs = Mathf.Abs(step);
            double decs = abs - (int)abs; // Strip away integer part
            for (int i = 0; i < sd.Length; i++)
            {
                if (decs >= sd[i])
                {
                    return i;
                }
            }
            return 0;
        }

        public static float Stepify(float s, float step)
        {
            if (step != 0f)
            {
                s = Floor(s / step + 0.5f) * step;
            }

            return s;
        }

        public static float Tan(float s)
        {
            return (float)Math.Tan(s);
        }

        public static float Tanh(float s)
        {
            return (float)Math.Tanh(s);
        }

        public static int Wrap(int value, int min, int max)
        {
            int range = max - min;
            return range == 0 ? min : min + ((value - min) % range + range) % range;
        }

        public static float Wrap(float value, float min, float max)
        {
            float range = max - min;
            return IsZeroApprox(range) ? min : min + ((value - min) % range + range) % range;
        }
    }
}
