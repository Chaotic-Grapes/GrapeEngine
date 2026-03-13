/* Start Header *****************************************************************/
/*!
\file   Vector4D.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
4D vector math implementation. Covers construction, arithmetic operators,
length and normalization, dot product, distance queries, interpolation,
clamping utilities, homogenization, point/vector W-flag helpers,
XYZ extraction and static direction constants.
Primarily used to represent homogeneous coordinates for 3D transform pipelines
where W = 1 denotes a point and W = 0 denotes a direction vector.
*/
/* End Header *******************************************************************/

#include "math/Vector4D.h"
#include <cmath>
#include <stdexcept>

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to zero; W = 0 means this is treated as a direction, not a point
Vector4D::Vector4D() : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f) {}

// Construct from explicit components; caller sets W to 1 for points or 0 for vectors
Vector4D::Vector4D(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

// ============================================================================
// Assignment operators (member functions)
// ============================================================================

// Element-wise addition-assign
Vector4D& Vector4D::operator+=(const Vector4D& other) {
	X += other.X;
	Y += other.Y;
	Z += other.Z;
	W += other.W;
	return *this;
}

// Element-wise subtraction-assign
Vector4D& Vector4D::operator-=(const Vector4D& other) {
	X -= other.X;
	Y -= other.Y;
	Z -= other.Z;
	W -= other.W;
	return *this;
}

// Uniform scalar multiplication-assign
Vector4D& Vector4D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	Z *= scalar;
	W *= scalar;
	return *this;
}

// Uniform scalar division-assign; multiply by reciprocal to avoid dividing four times
Vector4D& Vector4D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");

	// Multiply by reciprocal rather than dividing four times
	float invScalar = 1.0f / scalar;
	X *= invScalar;
	Y *= invScalar;
	Z *= invScalar;
	W *= invScalar;
	return *this;
}

// ============================================================================
// Unary operator
// ============================================================================

// Return a negated copy; does not modify this vector
Vector4D Vector4D::operator-() const { return Vector4D(-X, -Y, -Z, -W); }

// ============================================================================
// Member functions
// ============================================================================

// 4D Euclidean length; use SquareLength for comparisons to avoid the sqrt
float Vector4D::Length() const { return sqrtf(X * X + Y * Y + Z * Z + W * W); }

// Squared length; cheaper than Length() and sufficient for magnitude comparisons
float Vector4D::SquareLength() const { return X * X + Y * Y + Z * Z + W * W; }

// Return a unit-length copy without modifying this vector;
// returns zero vector if this vector has no length to avoid a divide-by-zero
Vector4D Vector4D::Normalized() const {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector4D(X * invLength, Y * invLength, Z * invLength, W * invLength);
	}

	// Zero-length vector has no defined direction; return zero rather than NaN
	return Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
}

// Normalize this vector in place; no-op if already zero to avoid a divide-by-zero
void Vector4D::Normalize() {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
		Z *= invLength;
		W *= invLength;
	}
}

// Instance dot product; all four components contribute, including W
float Vector4D::Dot(const Vector4D& other) const { return X * other.X + Y * other.Y + Z * other.Z + W * other.W; }

// ============================================================================
// Static functions
// ============================================================================

// Dot product across all four dimensions; commonly used for plane equations (normal.dot(point) - d)
float Vector4D::Dot(const Vector4D& a, const Vector4D& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }

// Euclidean distance between two 4D points
float Vector4D::Distance(const Vector4D& a, const Vector4D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	float dw = a.W - b.W;
	return sqrtf(dx * dx + dy * dy + dz * dz + dw * dw);
}

// Squared distance; avoids sqrt and is sufficient when only relative distances matter
float Vector4D::SquareDistance(const Vector4D& a, const Vector4D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	float dw = a.W - b.W;
	return dx * dx + dy * dy + dz * dz + dw * dw;
}

// Linear interpolation: lerp(a, b, t) = a + (b - a) * t;
// t is clamped to [0, 1] so callers do not need to guard against overshooting
Vector4D Vector4D::Lerp(const Vector4D& a, const Vector4D& b, float t) {
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector4D(
		a.X + (b.X - a.X) * t,
		a.Y + (b.Y - a.Y) * t,
		a.Z + (b.Z - a.Z) * t,
		a.W + (b.W - a.W) * t
	);
}

// Convert a homogeneous coordinate back to Cartesian by dividing XYZ by W;
// used after perspective projection to recover the actual 3D point from clip space;
// if W is zero (direction vector) the input is returned unchanged to avoid a divide-by-zero
Vector4D Vector4D::Homogenize(const Vector4D& vector) {
	if (vector.W != 0.0f) {
		float invW = 1.0f / vector.W;
		return Vector4D(
			vector.X * invW,   // X / W
			vector.Y * invW,   // Y / W
			vector.Z * invW,   // Z / W
			1.0f               // W becomes 1 after perspective divide
		);
	}

	// W == 0 means this is a direction vector; division is undefined so return as-is
	return vector;
}

// Per-component clamp; each axis is clamped independently against its own min/max
Vector4D Vector4D::ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max) {
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	float clampedZ = (vector.Z < min.Z) ? min.Z : ((vector.Z > max.Z) ? max.Z : vector.Z);
	float clampedW = (vector.W < min.W) ? min.W : ((vector.W > max.W) ? max.W : vector.W);
	return Vector4D(clampedX, clampedY, clampedZ, clampedW);
}

// Scalar clamp utility; convenience wrapper so callers do not branch manually
float Vector4D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// ============================================================================
// W-flag helpers
// ============================================================================

// W == 1 signals a point in homogeneous space; translation affects it in matrix multiplication
bool Vector4D::IsPoint() const { return W == 1.0f; }

// W == 0 signals a direction vector; translation does not affect it in matrix multiplication
bool Vector4D::IsVector() const { return W == 0.0f; }

// Drop W and return just the spatial components; commonly used to read back a transformed position
Vector3D Vector4D::XYZ() const { return Vector3D(X, Y, Z); }

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

// Element-wise addition
Vector4D operator+(const Vector4D& a, const Vector4D& b) { return Vector4D(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W); }

// Element-wise subtraction
Vector4D operator-(const Vector4D& a, const Vector4D& b) { return Vector4D(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W); }

// Both orderings provided so scalar * vector and vector * scalar compile equally
Vector4D operator*(const Vector4D& vector, float scalar) { return Vector4D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar, vector.W * scalar); }
Vector4D operator*(float scalar, const Vector4D& vector) { return Vector4D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar, vector.W * scalar); }

// Scalar division; non-member so the operand order matches vector / scalar naturally
Vector4D operator/(const Vector4D& vector, float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector4D(vector.X * invScalar, vector.Y * invScalar, vector.Z * invScalar, vector.W * invScalar);
}

// Exact float comparison; callers should use distance checks for epsilon equality
bool operator==(const Vector4D& a, const Vector4D& b) { return (a.X == b.X) && (a.Y == b.Y) && (a.Z == b.Z) && (a.W == b.W); }

// Inequality via negated equality check
bool operator!=(const Vector4D& a, const Vector4D& b) { return !(a == b); }

// ============================================================================
// Static member definitions
// ============================================================================

// Zero vector with W = 0; represents a null direction in homogeneous space
const Vector4D Vector4D::Zero = Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
