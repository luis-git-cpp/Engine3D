#include "Coordinates.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

CoordinateSystem::CoordinateSystem(float cellSize)
    : m_cellSize(cellSize > 0.0f ? cellSize : 1.0f)
{
}

CellCoord CoordinateSystem::ToCell(Vector3 p) const
{
    CellCoord c;
    c.x = static_cast<int>(std::floor(p.x / m_cellSize));
    c.y = static_cast<int>(std::floor(p.y / m_cellSize));
    c.z = static_cast<int>(std::floor(p.z / m_cellSize));
    return c;
}

Vector3 CoordinateSystem::CellOrigin(CellCoord c) const
{
    return {c.x * m_cellSize, c.y * m_cellSize, c.z * m_cellSize};
}

Vector3 CoordinateSystem::CellCenter(CellCoord c) const
{
    const Vector3 o = CellOrigin(c);
    const float h = m_cellSize * 0.5f;
    return {o.x + h, o.y + h, o.z + h};
}

Vector3 CoordinateSystem::ToLocal(Vector3 p) const
{
    const Vector3 o = CellOrigin(ToCell(p));
    return {p.x - o.x, p.y - o.y, p.z - o.z};
}

Vector3 CoordinateSystem::ToWorld(CellCoord c, Vector3 local) const
{
    const Vector3 o = CellOrigin(c);
    return {o.x + local.x, o.y + local.y, o.z + local.z};
}

void CoordinateSystem::CellRange(const BoundingBox& box, CellCoord& minCell, CellCoord& maxCell) const
{
    minCell = ToCell(box.min);
    maxCell = ToCell(box.max);
}

int CoordinateSystem::CellDistance(CellCoord a, CellCoord b)
{
    return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
}
