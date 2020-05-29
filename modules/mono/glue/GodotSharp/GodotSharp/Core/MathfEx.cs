using System;

namespace Godot
{
    public static partial class Mathf
    {
        // Define constants with Decimal precision and cast down to double or float.

        public const float E = (float) 2.7182818284590452353602874714f; // 2.7182817f and 2.718281828459045
        public const float Sqrt2 = (float) 1.4142135623730950488016887242f; // 1.4142136f and 1.414213562373095

#if float_IS_DOUBLE
        public const float Epsilon = 1e-14; // Epsilon size should depend on the precision used.
#else
        public const float Epsilon = 1e-06f;
#endif

        public static int DecimalCount(float s)
        {
            return DecimalCount((decimal)s);
        }

        public static int DecimalCount(decimal s)
        {
            return BitConverter.GetBytes(decimal.GetBits(s)[3])[2];
        }

        public static int CeilToInt(float s)
        {
            return (int)Math.Ceiling(s);
        }

        public static int FloorToInt(float s)
        {
            return (int)Math.Floor(s);
        }

        public static int RoundToInt(float s)
        {
            return (int)Math.Round(s);
        }

        public static bool IsEqualApprox(float a, float b, float tolerance)
        {
            // Check for exact equality first, required to handle "infinity" values.
            if (a == b)
            {
                return true;
            }
            // Then check for approximate equality.
            return Abs(a - b) < tolerance;
        }
    }
}
