#pragma once

class Vector3D {
public:
    float X, Y, Z;

    // Constructors
    Vector3D();
    Vector3D(float x, float y, float z);
    Vector3D(const Vector3D& other) = default;
    Vector3D& operator=(const Vector3D& other) = default;

    // Assignment operators (member functions)
    Vector3D& operator+=(const Vector3D& other);
    Vector3D& operator-=(const Vector3D& other);
    Vector3D& operator*=(float scalar);
    Vector3D& operator/=(float scalar);

    // Unary operator
    Vector3D operator-() const;

    // Member functions
    float Length() const;
    float SquareLength() const;
    Vector3D Normalized() const;  // Returns a new vector
    void Normalize();             // Modifies the current vector
    float Dot(const Vector3D& other) const;

    // Static functions
    static float Dot(const Vector3D& a, const Vector3D& b);
    static Vector3D Cross(const Vector3D& a, const Vector3D& b);
    static float Distance(const Vector3D& a, const Vector3D& b);
    static float SquareDistance(const Vector3D& a, const Vector3D& b);
    static Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);
    static Vector3D ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max);
    static float ClampValue(float component, float min, float max);
};

// Binary operators (non-member functions)
Vector3D operator+(const Vector3D& a, const Vector3D& b);
Vector3D operator-(const Vector3D& a, const Vector3D& b);
Vector3D operator*(const Vector3D& vector, float scalar);
Vector3D operator*(float scalar, const Vector3D& vector);
Vector3D operator/(const Vector3D& vector, float scalar);
bool operator==(const Vector3D& a, const Vector3D& b);
bool operator!=(const Vector3D& a, const Vector3D& b);
