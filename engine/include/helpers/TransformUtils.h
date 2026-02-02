/* Start Header *****************************************************************/
/*!
\file   TransformUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares utility functions for composing and decomposing transformation matrices.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "math/Vector3D.h"
#include "math/Matrix4x4.h"
#include "math/Quaternion.h"

class TransformUtils {
public:
    static Matrix4x4 MakeRotation(const Quaternion& qIn) {
        Quaternion q = qIn;
        _safeNormalize(q);

        // Precompute products
        float xx = q.X * q.X;
        float yy = q.Y * q.Y;
        float zz = q.Z * q.Z;
        float xy = q.X * q.Y;
        float xz = q.X * q.Z;
        float yz = q.Y * q.Z;
        float wx = q.W * q.X;
        float wy = q.W * q.Y;
        float wz = q.W * q.Z;

        // Rotation portion (3x3), homogeneous last row/column
        return Matrix4x4(
            1.0f - 2.0f*(yy + zz), 2.0f*(xy - wz),        2.0f*(xz + wy),        0.0f,
            2.0f*(xy + wz),        1.0f - 2.0f*(xx + zz), 2.0f*(yz - wx),        0.0f,
            2.0f*(xz - wy),        2.0f*(yz + wx),        1.0f - 2.0f*(xx + yy), 0.0f,
            0.0f,                  0.0f,                  0.0f,                  1.0f
        );
    }

    // Compose Translation * Rotation * Scale (TRS)
    static Matrix4x4 MakeTRS(const Vector3D& position, const Quaternion& rotation, const Vector3D& scale) {
        Matrix4x4 t = Matrix4x4::Translation(position.X, position.Y, position.Z);

        // Scale matrix
        Matrix4x4 s(
            scale.X, 0,       0,       0,
            0,       scale.Y, 0,       0,
            0,       0,       scale.Z, 0,
            0,       0,       0,       1
        );

        Matrix4x4 r = MakeRotation(rotation);

        // Compose: T * (R * S)
        return t * (r * s);
    }

    // Decompose TRS (assumes no shear, uniform basis)
    static void DecomposeTRS(const Matrix4x4& m, Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) {
        // Matrix4x4::Translation stores translation in the last column (m03/m13/m23).
        outPosition = Vector3D(m.m03, m.m13, m.m23);

        // Extract scale from basis vectors (columns of rotation-scale in M = T * R * S).
        Vector3D basisX(m.m00, m.m10, m.m20);
        Vector3D basisY(m.m01, m.m11, m.m21);
        Vector3D basisZ(m.m02, m.m12, m.m22);

        float sx = std::sqrt(basisX.X*basisX.X + basisX.Y*basisX.Y + basisX.Z*basisX.Z);
        float sy = std::sqrt(basisY.X*basisY.X + basisY.Y*basisY.Y + basisY.Z*basisY.Z);
        float sz = std::sqrt(basisZ.X*basisZ.X + basisZ.Y*basisZ.Y + basisZ.Z*basisZ.Z);
        outScale = Vector3D(sx, sy, sz);

        if (sx > 0.0f) { basisX = basisX / sx; }
        if (sy > 0.0f) { basisY = basisY / sy; }
        if (sz > 0.0f) { basisZ = basisZ / sz; }

        // Reconstruct quaternion from rotation matrix (row-major 3x3 elements m00..m22).
        const float m00 = basisX.X;
        const float m01 = basisY.X;
        const float m02 = basisZ.X;
        const float m10 = basisX.Y;
        const float m11 = basisY.Y;
        const float m12 = basisZ.Y;
        const float m20 = basisX.Z;
        const float m21 = basisY.Z;
        const float m22 = basisZ.Z;

        float trace = m00 + m11 + m22;
        Quaternion q;
        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.W = 0.25f * s;
            q.X = (m21 - m12) / s;
            q.Y = (m02 - m20) / s;
            q.Z = (m10 - m01) / s;
        }
        else if (m00 > m11 && m00 > m22) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.W = (m21 - m12) / s;
            q.X = 0.25f * s;
            q.Y = (m01 + m10) / s;
            q.Z = (m02 + m20) / s;
        }
        else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.W = (m02 - m20) / s;
            q.X = (m01 + m10) / s;
            q.Y = 0.25f * s;
            q.Z = (m12 + m21) / s;
        }
        else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.W = (m10 - m01) / s;
            q.X = (m02 + m20) / s;
            q.Y = (m12 + m21) / s;
            q.Z = 0.25f * s;
        }

        _safeNormalize(q);
        outRotation = q;
    }

private:
    static inline void _safeNormalize(Quaternion& q) {
        float len2 = q.X*q.X + q.Y*q.Y + q.Z*q.Z + q.W*q.W;
        
        // Normalize if length is non-zero
        if (len2 > 0.0f) {
            float inv = 1.0f / std::sqrt(len2);
            q.X *= inv;
            q.Y *= inv;
            q.Z *= inv;
            q.W *= inv;
        } 
        else {
            q = Quaternion::Identity();
        }
    }
};
