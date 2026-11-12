#pragma once

#include <cstddef>

#include "raylib.h"

// Coordenada de celula na grade do mundo (inteiros). Cada celula e um cubo de lado CellSize.
struct CellCoord
{
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const CellCoord& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const CellCoord& o) const { return !(*this == o); }
};

struct CellCoordHash
{
    std::size_t operator()(const CellCoord& c) const noexcept
    {
        std::size_t h = static_cast<std::size_t>(c.x) * 73856093u;
        h ^= static_cast<std::size_t>(c.y) * 19349663u;
        h ^= static_cast<std::size_t>(c.z) * 83492791u;
        return h;
    }
};

// Sistema de coordenadas do mundo: posicao em metros (float) + celula da grade (inteiros)
// + posicao local dentro da celula. Base para carregar so o que esta perto (streaming).
class CoordinateSystem
{
public:
    explicit CoordinateSystem(float cellSize = 8.0f);

    float CellSize() const { return m_cellSize; }

    CellCoord ToCell(Vector3 worldPosition) const;
    Vector3 CellOrigin(CellCoord cell) const;      // canto minimo da celula, em coordenadas do mundo
    Vector3 CellCenter(CellCoord cell) const;
    Vector3 ToLocal(Vector3 worldPosition) const;  // posicao dentro da propria celula, em [0, CellSize)
    Vector3 ToWorld(CellCoord cell, Vector3 local) const;

    // Faixa de celulas tocada por uma caixa (limites inclusivos).
    void CellRange(const BoundingBox& box, CellCoord& minCell, CellCoord& maxCell) const;

    // Distancia entre duas celulas, contada em celulas (maior diferenca entre os eixos).
    static int CellDistance(CellCoord a, CellCoord b);

private:
    float m_cellSize;
};
