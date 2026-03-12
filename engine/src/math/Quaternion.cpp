/* Start Header *****************************************************************/
/*!
\file   Quaternion.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   12th March 2026
\brief
Quaternion math implementation. Covers construction, length and normalization,
conjugate and inverse, Hamilton product, axis-angle and Euler angle construction,
spherical linear interpolation (Slerp), vector rotation and comparison operators.
Quaternions represent 3D rotations as (Xi + Yj + Zk + W) where W is the scalar part;
a unit quaternion encodes a rotation of 2*acos(W) radians around the axis (X, Y, Z).
*/
/* End Header *******************************************************************/

#include "math/Quaternion.h"
#include <cmath>

#define M_PI 3.14159265358979323846

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to the identity rotation: no rotation applied (axis irrelevant, angle = 0)
Quaternion::Quaternion() : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f) {}

// Construct from explicit components; caller is responsible for ensuring unit length if needed
Quaternion::Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

// ============================================================================
// Static constructors
// ============================================================================

// Return the identity quaternion; represents zero rotation and is the multiplicative identity
Quaternion Quaternion::Identity() { return Quaternion(0, 0, 0, 1); }

// ============================================================================
// Length and normalization
// ============================================================================

// Euclidean length of the quaternion 4-vector; for a valid rotation quaternion this should be 1
float Quaternion::Length() const { return std::sqrt(X * X + Y * Y + Z * Z + W * W); }

// Squared length; cheaper than Length() and sufficient for near-unit checks
float Quaternion::SquareLength() const { return X * X + Y * Y + Z * Z + W * W; }

// Normalize in place; divides each component by the length to produce a unit quaternion;
// no-op if length is zero to avoid a divide-by-zero on a degenerate quaternion
void Quaternion::Normalize() {
	float len = Length();

	if (len > 0.0f) {
		float inv = 1.0f / len;
		X *= inv; Y *= inv; Z *= inv; W *= inv;
	}
}

// Return a normalized copy; returns identity if this quaternion has zero length
// so callers always get a valid rotation back rather than NaN components
Quaternion Quaternion::Normalized() const {
	float len = Length();

	if (len > 0.0f) {
		float inv = 1.0f / len;
		return Quaternion(X * inv, Y * inv, Z * inv, W * inv);
	}

	// Degenerate quaternion has no direction; identity is the safest fallback
	return Quaternion::Identity();
}

// ============================================================================
// Conjugate and inverse
// ============================================================================

// Conjugate negates the vector (XYZ) part while keeping the scalar (W) part;
// for a unit quaternion, conjugate == inverse (cheaper path, no division needed)
Quaternion Quaternion::Conjugate() const { return Quaternion(-X, -Y, -Z, W); }

// Inverse: q^-1 = conjugate(q) / |q|^2; for unit quaternions this reduces to the conjugate;
// returns identity if the quaternion has zero length to avoid a divide-by-zero
Quaternion Quaternion::Inverse() const {
	float sq = SquareLength();

	if (sq > 0.0f) {
		// Scale the conjugate by 1/|q|^2 to undo the quaternion's magnitude
		float inv = 1.0f / sq;
		Quaternion c = Conjugate();
		return Quaternion(c.X * inv, c.Y * inv, c.Z * inv, c.W * inv);
	}

	return Quaternion::Identity();
}

// ============================================================================
// Multiplication
// ============================================================================

// Hamilton product: encodes concatenated rotations; a followed by b means b's rotation
// is applied first in world space (or a's first in local space depending on convention);
// the formula expands the distributive product of (aW + aX*i + aY*j + aZ*k) * (bW + bX*i + bY*j + bZ*k)
// using the quaternion identities: i^2 = j^2 = k^2 = ijk = -1, ij = k, jk = i, ki = j
Quaternion Quaternion::Multiply(const Quaternion& a, const Quaternion& b) {
	return Quaternion(
		a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,   // i component
		a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,   // j component
		a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,   // k component
		a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z    // scalar component
	);
}

// ============================================================================
// Factory methods
// ============================================================================

// Build a quaternion that rotates by angleRadians around the given axis;
// the formula is: q = (axis * sin(angle/2), cos(angle/2)); axis must be normalized by the caller
Quaternion Quaternion::FromAxisAngle(const Vector3D& axis, float angleRadians) {
	float half = angleRadians * 0.5f;
	float s = std::sin(half);   // sin(half-angle) scales the axis to encode rotation magnitude
	float c = std::cos(half);   // cos(half-angle) is the scalar part W
	return Quaternion(axis.X * s, axis.Y * s, axis.Z * s, c);
}

// Build a quaternion from Euler angles in ZYX application order (roll, then yaw, then pitch);
// each axis angle is converted to a quaternion then multiplied together;
// the final normalize guards against accumulated floating-point drift in the half-angle trig
Quaternion Quaternion::FromEulerRad(float pitchX, float yawY, float rollZ) {
	// Precompute half-angle sin/cos for each axis to keep the quaternion construction clean
	float cx = std::cos(pitchX * 0.5f), sx = std::sin(pitchX * 0.5f);
	float cy = std::cos(yawY * 0.5f), sy = std::sin(yawY * 0.5f);
	float cz = std::cos(rollZ * 0.5f), sz = std::sin(rollZ * 0.5f);

	// Each quaternion represents a single-axis rotation via FromAxisAngle logic
	Quaternion qx(sx, 0, 0, cx);   // pitch around X
	Quaternion qy(0, sy, 0, cy);   // yaw around Y
	Quaternion qz(0, 0, sz, cz);   // roll around Z

	// ZYX application order: roll first, then yaw, then pitch; read right to left in world space
	return Multiply(Multiply(qx, qy), qz).Normalized();
}

// ============================================================================
// Interpolation
// ============================================================================

// Spherical linear interpolation between two unit quaternions at parameter t in [0, 1];
// Slerp travels along the great arc on the 4D unit sphere, producing constant angular velocity;
// falls back to normalized linear interpolation when the quaternions are nearly collinear
// (dot > threshold) to avoid a divide-by-zero in the sin terms
Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
	// Clamp t to avoid extrapolation beyond the endpoints
	if (t <= 0.0f) return a;
	if (t >= 1.0f) return b;

	// Dot product measures the cosine of the angle between the two quaternions in 4D;
	// values close to 1 mean nearly the same rotation, values close to -1 mean opposite hemisphere
	float dot = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
	Quaternion bb = b;

	// If dot is negative the two quaternions are in opposite hemispheres;
	// negating b flips it to the same hemisphere so Slerp takes the shorter arc
	if (dot < 0.0f) {
		dot = -dot;
		bb = Quaternion(-b.X, -b.Y, -b.Z, -b.W);
	}

	// When the quaternions are nearly identical the angle is close to zero;
	// sin(theta) approaches zero, making the Slerp formula numerically unstable,
	// so we fall back to normalized lerp which is accurate enough at tiny angles
	const float DOT_THRESHOLD = 0.9995f;

	if (dot > DOT_THRESHOLD) {
		Quaternion result(
			a.X + t * (bb.X - a.X),
			a.Y + t * (bb.Y - a.Y),
			a.Z + t * (bb.Z - a.Z),
			a.W + t * (bb.W - a.W)
		);
		return result.Normalized();
	}

	// Standard Slerp formula: result = (sin((1-t)*theta)/sin(theta)) * a + (sin(t*theta)/sin(theta)) * b
	float theta0 = std::acos(dot);          // full angle between a and b
	float theta = theta0 * t;              // fraction of that angle to travel
	float sinTheta = std::sin(theta);
	float sinTheta0 = std::sin(theta0);

	// s0 and s1 are the blending weights that maintain constant angular velocity;
	// this formulation from Shoemake 1985 is numerically stable for non-degenerate cases
	float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
	float s1 = sinTheta / sinTheta0;

	return Quaternion(
		s0 * a.X + s1 * bb.X,
		s0 * a.Y + s1 * bb.Y,
		s0 * a.Z + s1 * bb.Z,
		s0 * a.W + s1 * bb.W
	);
}

// ============================================================================
// Vector rotation
// ============================================================================

// Rotate a vector by this quaternion using the sandwich product: v' = q * (v,0) * q^-1;
// wrapping v in a pure quaternion (W=0) allows the Hamilton product to apply the rotation;
// the imaginary part of the result is the rotated vector
Vector3D Quaternion::Rotate(const Vector3D& v) const {
	Quaternion qv(v.X, v.Y, v.Z, 0.0f);   // pure quaternion encoding the vector
	Quaternion inv = Inverse();
	Quaternion res = Multiply(Multiply(*this, qv), inv);
	return Vector3D(res.X, res.Y, res.Z);
}

// ============================================================================
// Comparison operators
// ============================================================================

// Exact component-wise equality; use an epsilon comparison for floating-point-safe checks
bool Quaternion::operator==(const Quaternion& other) const {
	return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
}

// Inequality via negated equality check
bool Quaternion::operator!=(const Quaternion& other) const { return !(*this == other); }

// ============================================================================
// Non-member operator
// ============================================================================

// Delegate to Multiply so quaternion composition can be written as q1 * q2
Quaternion operator*(const Quaternion& a, const Quaternion& b) {
	return Quaternion::Multiply(a, b);
}
