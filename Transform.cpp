#include "Transform.h"

#include "raymath.h"

namespace engine
{
Matrix Transform::ToMatrix(const Matrix& modelLocal) const
{
    const Matrix s = MatrixScale(scale.x, scale.y, scale.z);
    const Matrix r = MatrixRotateXYZ({DEG2RAD * rotation.x, DEG2RAD * rotation.y, DEG2RAD * rotation.z});
    const Matrix t = MatrixTranslate(position.x, position.y, position.z);
    return MatrixMultiply(modelLocal, MatrixMultiply(MatrixMultiply(s, r), t));
}
}