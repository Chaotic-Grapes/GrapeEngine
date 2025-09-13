#include "Vector2D.h"

// -------------------------
// Constructors
// -------------------------
Vector2D::Vector2D() : x(0.0f), y(0.0f) {}
Vector2D::Vector2D(float _x, float _y) : x(_x), y(_y) {}

// -------------------------
// In-place arithmetic
// -------------------------
Vector2D& Vector2D::operator+=(const Vector2D& rhs) { x += rhs.x; y += rhs.y; return *this; }
Vector2D& Vector2D::operator-=(const Vector2D& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
Vector2D& Vector2D::operator*=(float rhs) { x *= rhs;   y *= rhs;   return *this; }
Vector2D& Vector2D::operator/=(float rhs) { x /= rhs;   y /= rhs;   return *this; } // avoid rhs==0

// -------------------------
// Unary minus
// -------------------------
Vector2D Vector2D::operator-() const { return Vector2D(-x, -y); }


// -------------------------
// Queries (GLM-backed)
// -------------------------
float Vector2D::SquareLength() const {
    glm::vec2 v(x, y);
    return glm::dot(v, v);
}

float Vector2D::Length() const {
    return glm::length(glm::vec2(x, y));
}

float Vector2D::SquareDistance(const Vector2D& other) const {
    glm::vec2 d(x - other.x, y - other.y);
    return glm::dot(d, d);
}

float Vector2D::Distance(const Vector2D& other) const {
    return glm::distance(glm::vec2(x, y), glm::vec2(other.x, other.y));
}

float Vector2D::Dot(const Vector2D& other) const {
    return glm::dot(glm::vec2(x, y), glm::vec2(other.x, other.y));
}

float Vector2D::CrossProductMag(const Vector2D& other) const {
    // 2D cross magnitude
    return x * other.y - y * other.x;
}

// -------------------------
// Normalization
// -------------------------
void Vector2D::Normalize() {
    glm::vec2 v(x, y);
    float L2 = glm::dot(v, v);
    if (L2 == 0.0f) { x = 0.0f; y = 0.0f; return; }
    glm::vec2 n = glm::normalize(v);
    x = n.x; y = n.y;
}

Vector2D Vector2D::Normalized() const {
    glm::vec2 v(x, y);
    float L2 = glm::dot(v, v);
    if (L2 == 0.0f) return Vector2D(0.0f, 0.0f);
    glm::vec2 n = glm::normalize(v);
    return Vector2D(n.x, n.y);
}

// -------------------------
// Clamp utilities (GLM-backed)
// -------------------------
Vector2D Vector2D::Clamp(const Vector2D& v, const Vector2D& lo, const Vector2D& hi) {
    glm::vec2 c = glm::clamp(glm::vec2(v.x, v.y),
        glm::vec2(lo.x, lo.y),
        glm::vec2(hi.x, hi.y));
    return Vector2D(c.x, c.y);
}

float Vector2D::ClampFloat(float x, float lo, float hi) {
    return glm::clamp(x, lo, hi);
}

// -------------------------
// Free operators
// -------------------------
Vector2D operator+(const Vector2D& lhs, const Vector2D& rhs) { return Vector2D(lhs.x + rhs.x, lhs.y + rhs.y); }
Vector2D operator-(const Vector2D& lhs, const Vector2D& rhs) { return Vector2D(lhs.x - rhs.x, lhs.y - rhs.y); }
Vector2D operator*(const Vector2D& lhs, float rhs) { return Vector2D(lhs.x * rhs, lhs.y * rhs); }
Vector2D operator*(float lhs, const Vector2D& rhs) { return Vector2D(lhs * rhs.x, lhs * rhs.y); }
Vector2D operator/(const Vector2D& lhs, float rhs) { return Vector2D(lhs.x / rhs, lhs.y / rhs); } // avoid rhs==0
