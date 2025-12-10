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
        // Extract translation (assuming it is stored in the last row or column depends on convention)
        // Here we assume translation is in the last row m30..m32 for row-major; if using last column m03..m23, adapt as needed
        // If Matrix4x4::Translation uses last column, swap the extraction accordingly

        // Heuristic: test magnitudes of (m30,m31,m32) vs (m03,m13,m23) to decide where translation likely is
        float rowTransMag2 = m.m30*m.m30 + m.m31*m.m31 + m.m32*m.m32;
        float colTransMag2 = m.m03*m.m03 + m.m13*m.m13 + m.m23*m.m23;
        bool useRow = rowTransMag2 >= colTransMag2;

        if (useRow) {
            outPosition = Vector3D(m.m30, m.m31, m.m32);
        }
        else {
            outPosition = Vector3D(m.m03, m.m13, m.m23);
        }

        // Extract scale from basis vectors (columns of rotation-scale if column-major, rows if row-major)
        // Read as if the rotation-scale is in upper-left 3x3 in row-major storage
        Vector3D basisX(m.m00, m.m01, m.m02);
        Vector3D basisY(m.m10, m.m11, m.m12);
        Vector3D basisZ(m.m20, m.m21, m.m22);

        float sx = std::sqrt(basisX.X*basisX.X + basisX.Y*basisX.Y + basisX.Z*basisX.Z);
        float sy = std::sqrt(basisY.X*basisY.X + basisY.Y*basisY.Y + basisY.Z*basisY.Z);
        float sz = std::sqrt(basisZ.X*basisZ.X + basisZ.Y*basisZ.Y + basisZ.Z*basisZ.Z);
        outScale = Vector3D(sx, sy, sz);

        if (sx > 0.0f) { basisX = basisX / sx; }
        if (sy > 0.0f) { basisY = basisY / sy; }
        if (sz > 0.0f) { basisZ = basisZ / sz; }

        // Reconstruct quaternion from rotation matrix (row-major, 3x3)
        float trace = basisX.X + basisY.Y + basisZ.Z;
        Quaternion q;
        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.W = 0.25f * s;
            q.X = (basisZ.Y - basisY.Z) / s;
            q.Y = (basisX.Z - basisZ.X) / s;
            q.Z = (basisY.X - basisX.Y) / s;
        }
        else if (basisX.X > basisY.Y && basisX.X > basisZ.Z) {
            float s = std::sqrt(1.0f + basisX.X - basisY.Y - basisZ.Z) * 2.0f;
            q.W = (basisZ.Y - basisY.Z) / s;
            q.X = 0.25f * s;
            q.Y = (basisX.Y + basisY.X) / s;
            q.Z = (basisX.Z + basisZ.X) / s;
        }
        else if (basisY.Y > basisZ.Z) {
            float s = std::sqrt(1.0f + basisY.Y - basisX.X - basisZ.Z) * 2.0f;
            q.W = (basisX.Z - basisZ.X) / s;
            q.X = (basisX.Y + basisY.X) / s;
            q.Y = 0.25f * s;
            q.Z = (basisY.Z + basisZ.Y) / s;
        }
        else {
            float s = std::sqrt(1.0f + basisZ.Z - basisX.X - basisY.Y) * 2.0f;
            q.W = (basisY.X - basisX.Y) / s;
            q.X = (basisX.Z + basisZ.X) / s;
            q.Y = (basisY.Z + basisZ.Y) / s;
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