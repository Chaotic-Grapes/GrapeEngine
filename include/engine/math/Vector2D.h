#pragma once

class Vector2D {
public:
    float X, Y;

    // Constructors
    Vector2D();
    Vector2D(float x, float y);
    Vector2D(const Vector2D& other) = default;
    Vector2D& operator=(const Vector2D& other) = default;

    // Assignment operators (member functions)
    Vector2D& operator+=(const Vector2D& other);
    Vector2D& operator-=(const Vector2D& other);
    Vector2D& operator*=(float scalar);
    Vector2D& operator/=(float scalar);

    // Unary operator
    Vector2D operator-() const;

    // Member functions
    float Length() const;
    float SquareLength() const;
    Vector2D Normalized() const;  // Returns a new vector
    void Normalize();             // Modifies the current vector
    float Dot(const Vector2D& other) const;

    // Static functions
    static float Dot(const Vector2D& a, const Vector2D& b);
    static float Cross(const Vector2D& a, const Vector2D& b);
    static float Distance(const Vector2D& a, const Vector2D& b);
    static float SquareDistance(const Vector2D& a, const Vector2D& b);
    static Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);
    static Vector2D ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max);
    static float ClampValue(float component, float min, float max);

    // Static members
    static const Vector2D Zero;
    static const Vector2D Up;
    static const Vector2D Right;
    static const Vector2D Down;
    static const Vector2D Left;

    // To Vector3D conversion
};

// Binary operators (non-member functions)
Vector2D operator+(const Vector2D& a, const Vector2D& b);
Vector2D operator-(const Vector2D& a, const Vector2D& b);
Vector2D operator*(const Vector2D& vector, float scalar);
Vector2D operator*(float scalar, const Vector2D& vector);
Vector2D operator/(const Vector2D& vector, float scalar);
bool operator==(const Vector2D& a, const Vector2D& b);
bool operator!=(const Vector2D& a, const Vector2D& b);

// Static member definitions
const Vector2D Vector2D::Zero = Vector2D(0.0f, 0.0f);
const Vector2D Vector2D::Up   = Vector2D(0.0f, 1.0f);
const Vector2D Vector2D::Right= Vector2D(1.0f, 0.0f);
const Vector2D Vector2D::Down = Vector2D(0.0f, -1.0f);
const Vector2D Vector2D::Left = Vector2D(-1.0f, 0.0f);