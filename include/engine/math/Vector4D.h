#pragma once
#include "Math/Vector3D.h"

class Vector4D {
public:
    float X, Y, Z, W;

    // Constructors
    Vector4D();
    Vector4D(float x, float y, float z, float w);
    Vector4D(const Vector4D& other) = default;
    Vector4D& operator=(const Vector4D& other) = default;

    // Assignment operators (member functions)
    Vector4D& operator+=(const Vector4D& other);
    Vector4D& operator-=(const Vector4D& other);
    Vector4D& operator*=(float scalar);
    Vector4D& operator/=(float scalar);

    // Unary operator
    Vector4D operator-() const;

    // Member functions
    float Length() const;
    float SquareLength() const;
    Vector4D Normalized() const;  // Returns a new vector
    void Normalize();             // Modifies the current vector
    float Dot(const Vector4D& other) const;

    // Static functions
    static float Dot(const Vector4D& a, const Vector4D& b);
    static float Distance(const Vector4D& a, const Vector4D& b);
    static float SquareDistance(const Vector4D& a, const Vector4D& b);
    static Vector4D Lerp(const Vector4D& a, const Vector4D& b, float t);
    static Vector4D Homogenize(const Vector4D& vector);
    static Vector4D ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max);
    static float ClampValue(float component, float min, float max);

    // Some other functions
    bool IsPoint() const;   // If W == 1.0f
    bool IsVector() const;  // If W == 0.0f
    Vector3D XYZ() const;   // Extract 3D components

    // Static members
    static const Vector4D Zero;
};

// Binary operators (non-member functions)
Vector4D operator+(const Vector4D& a, const Vector4D& b);
Vector4D operator-(const Vector4D& a, const Vector4D& b);
Vector4D operator*(const Vector4D& vector, float scalar);
Vector4D operator*(float scalar, const Vector4D& vector);
Vector4D operator/(const Vector4D& vector, float scalar);
bool operator==(const Vector4D& a, const Vector4D& b);
bool operator!=(const Vector4D& a, const Vector4D& b);

// Static member definitions
inline const Vector4D Vector4D::Zero = Vector4D(0.0f, 0.0f, 0.0f, 0.0f);