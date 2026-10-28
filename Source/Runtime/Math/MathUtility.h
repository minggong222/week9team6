#ifndef MATH_UTILITY_INCLUDED
#define MATH_UTILITY_INCLUDED

// Mathematical constants used by C++ code.
#define INDEX_NONE (-1)

// Preserve the original engine PI precision; rendering uses the precise variant.
#define PI (3.141592f)
#define PI_PRECISE (3.14159265358979323846f)
#define TWO_PI (6.2831853071795864769f)
#define HALF_CIRCLE_DEGREES (180.0f)
#define DEGREES_TO_RADIANS (PI / HALF_CIRCLE_DEGREES)
#define RADIANS_TO_DEGREES (HALF_CIRCLE_DEGREES / PI)

// General-purpose absolute tolerances; squared quantities need their own scale.
#define EPSILON_1E_4 (1.e-4f)
#define EPSILON_1E_6 (1.e-6f)
#define EPSILON_1E_8 (1.e-8f)
#define EPSILON_1E_12 (1.e-12f)

// Algorithm-specific limits retain their original scales and precision.
#define SHADOW_CASTER_MIN_DISTANCE (1.e-3f)
#define THRESH_MATRIX_INVERSE_DETERMINANT (1.e-20f)
#define LOD_MIN_TRIANGLE_CROSS_SQUARED (1.e-20f)
#define LOD_MIN_EDGE_LENGTH_SQUARED (1.e-20f)
#define LOD_MIN_QEM_CURVATURE (1.e-20)
#define LOD_QEM_RELATIVE_PIVOT_TOLERANCE (1.e-12)
#define LOD_MIN_REFERENCE_VOLUME (1.e-12)
#define MESH_BOUNDS_RELATIVE_MARGIN (2.e-5)

#define THRESH_NORMALS_ARE_PARALLEL (0.999845f)
#define THRESH_NORMALS_ARE_SMOOTH (0.9999f)
#define THRESH_SLIDING_DOT (0.01f)
#define THRESH_LOOK_AT_PARALLEL (0.999f)
#define THRESH_SAFE_AXIS_ALIGNMENT (0.9f)
#define DOUBLE_THRESH_SAFE_AXIS_ALIGNMENT (0.9)

#endif // MATH_UTILITY_INCLUDED
