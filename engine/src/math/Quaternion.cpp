/* Start Header *****************************************************************/
/*!
\file   Quaternion.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Quaternion math utilities and operations.
*/
/* End Header *******************************************************************/

#include "math/Quaternion.h"
#include <cmath>

# define M_PI 3.14159265358979323846

// Construct an identity quaternion.
Quaternion::Quaternion() : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f) {}
// Construct a quaternion from explicit components.
Quaternion::Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

// Return the identity quaternion.
Quaternion Quaternion::Identity() { return Quaternion(0,0,0,1); }

// Return the quaternion length.
float Quaternion::Length() const { return std::sqrt(X*X + Y*Y + Z*Z + W*W); }
// Return the squared quaternion length.
float Quaternion::SquareLength() const { return X*X + Y*Y + Z*Z + W*W; }

// Normalize the quaternion in place.
void Quaternion::Normalize() {
    float len = Length();
    if (len > 0.0f) {
        float inv = 1.0f / len;
        X *= inv; Y *= inv; Z *= inv; W *= inv;
    }
}
// Return a normalized copy of the quaternion.
Quaternion Quaternion::Normalized() const {
    float len = Length();
    if (len > 0.0f) {
        float inv = 1.0f / len;
        return Quaternion(X*inv, Y*inv, Z*inv, W*inv);
    }
    return Quaternion::Identity();
}

// Return the conjugate of the quaternion.
Quaternion Quaternion::Conjugate() const { return Quaternion(-X, -Y, -Z, W); }

// Return the inverse quaternion (identity if near zero length).
Quaternion Quaternion::Inverse() const {
    float sq = SquareLength();
    if (sq > 0.0f) {
        float inv = 1.0f / sq;
        Quaternion c = Conjugate();
        return Quaternion(c.X * inv, c.Y * inv, c.Z * inv, c.W * inv);
    }
    return Quaternion::Identity();
}

// Multiply two quaternions (Hamilton product).
Quaternion Quaternion::Multiply(const Quaternion& a, const Quaternion& b) {
    // Hamilton product (a followed by b)
    return Quaternion(
        a.W*b.X + a.X*b.W + a.Y*b.Z - a.Z*b.Y,
        a.W*b.Y - a.X*b.Z + a.Y*b.W + a.Z*b.X,
        a.W*b.Z + a.X*b.Y - a.Y*b.X + a.Z*b.W,
        a.W*b.W - a.X*b.X - a.Y*b.Y - a.Z*b.Z
    );
}

// Create a quaternion from an axis-angle rotation.
Quaternion Quaternion::FromAxisAngle(const Vector3D& axis, float angleRadians) {
    float half = angleRadians * 0.5f;
    float s = std::sin(half);
    float c = std::cos(half);
    // Assume axis is normalized by caller; clamp if needed
    return Quaternion(axis.X * s, axis.Y * s, axis.Z * s, c);
}

// Create a quaternion from Euler angles (radians).
Quaternion Quaternion::FromEulerRad(float pitchX, float yawY, float rollZ) {
    // ZYX convention: roll around Z, yaw around Y, pitch around X
    float cx = std::cos(pitchX * 0.5f), sx = std::sin(pitchX * 0.5f);
    float cy = std::cos(yawY   * 0.5f), sy = std::sin(yawY   * 0.5f);
    float cz = std::cos(rollZ  * 0.5f), sz = std::sin(rollZ  * 0.5f);

    Quaternion qx(sx, 0, 0, cx);
    Quaternion qy(0, sy, 0, cy);
    Quaternion qz(0, 0, sz, cz);
    // Note: order matters; apply roll, then yaw, then pitch: q = qx * qy * qz
    return Multiply(Multiply(qx, qy), qz).Normalized();
}

// Perform spherical linear interpolation between two quaternions.
Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
    // Clamp t
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    // Compute the cosine of the angle between the two quaternions
    float dot = a.X*b.X + a.Y*b.Y + a.Z*b.Z + a.W*b.W;
    Quaternion bb = b;

    // If negative dot, negate b to take the shortest path
    if (dot < 0.0f) {
        dot = -dot;
        bb = Quaternion(-b.X, -b.Y, -b.Z, -b.W);
    }

    const float DOT_THRESHOLD = 0.9995f;
    if (dot > DOT_THRESHOLD) {
        // Linear interpolate and normalize
        Quaternion result(
            a.X + t*(bb.X - a.X),
            a.Y + t*(bb.Y - a.Y),
            a.Z + t*(bb.Z - a.Z),
            a.W + t*(bb.W - a.W)
        );
        return result.Normalized();
    }

    float theta0 = std::acos(dot);
    float theta = theta0 * t;
    float sinTheta = std::sin(theta);
    float sinTheta0 = std::sin(theta0);

    float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    float s1 = sinTheta / sinTheta0;

    return Quaternion(
        s0 * a.X + s1 * bb.X,
        s0 * a.Y + s1 * bb.Y,
        s0 * a.Z + s1 * bb.Z,
        s0 * a.W + s1 * bb.W
    );
}

// Rotate a vector by this quaternion.
Vector3D Quaternion::Rotate(const Vector3D& v) const {
    // v' = q * (v,0) * q^{-1}
    Quaternion qv(v.X, v.Y, v.Z, 0.0f);
    Quaternion inv = Inverse();
    Quaternion res = Multiply(Multiply(*this, qv), inv);
    return Vector3D(res.X, res.Y, res.Z);
}

// Compare quaternions for exact equality.
bool Quaternion::operator==(const Quaternion& other) const {
    return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
}
// Compare quaternions for exact inequality.
bool Quaternion::operator!=(const Quaternion& other) const { return !(*this == other); }

// Multiply two quaternions.
Quaternion operator*(const Quaternion& a, const Quaternion& b) {
    return Quaternion::Multiply(a, b);
}
