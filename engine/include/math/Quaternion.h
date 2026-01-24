#pragma once
#include "math/Vector3D.h"
#include "Export.h"

class GRAPEENGINE_API Quaternion {
public:
    float X, Y, Z, W;

    // Constructors
    Quaternion();
    Quaternion(float x, float y, float z, float w);

    // Identity
    static Quaternion Identity();

    // Basic operations
    float Length() const;
    float SquareLength() const;
    void Normalize();
    Quaternion Normalized() const;
    Quaternion Conjugate() const;
    Quaternion Inverse() const;

    // Composition
    static Quaternion Multiply(const Quaternion& a, const Quaternion& b);
    static Quaternion FromAxisAngle(const Vector3D& axis, float angleRadians);
    static Quaternion FromEulerRad(float pitchX, float yawY, float rollZ);
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

    // Rotate a vector
    Vector3D Rotate(const Vector3D& v) const;

    // Comparisons
    bool operator==(const Quaternion& other) const;
    bool operator!=(const Quaternion& other) const;
};

GRAPEENGINE_API Quaternion operator*(const Quaternion& a, const Quaternion& b);