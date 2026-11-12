#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ObjectManager.h"
#include "raylib.h"

// Um nível de detalhe: ou uma primitiva, ou um modelo externo.
struct LodLevel
{
    enum class Kind { Primitive, ModelFile } kind = Kind::Primitive;

    // Primitiva
    PrimitiveType primitive = PrimitiveType::Cube;
    Vector3 size{1.0f, 1.0f, 1.0f};

    // Modelo externo
    std::string modelPath;

    // Opcional: tint específico do LOD (ex: LOD distante sem cor)
    std::optional<Color> tint;
};

// Especificação de LODs para um objeto do streaming.
// lods[0] = mais próximo, lods.back() = mais distante.
// distances[i] = distância (em metros) a partir da qual o LOD i+1 deve ser usado.
//   - distances.size() deve ser == lods.size() - 1.
//   - Ex: lods = {LOD0, LOD1, LOD2}, distances = {40.0f, 120.0f}
//       dist < 40       -> LOD0
//       40 <= dist < 120 -> LOD1
//       dist >= 120     -> LOD2
struct LodSpec
{
    std::vector<LodLevel> lods;
    std::vector<float> distances; // em metros
    float hysteresis = 5.0f;      // margem (m) para trocar de LOD, evita flicker

    bool IsValid() const
    {
        return !lods.empty() && distances.size() == lods.size() - 1;
    }

    // Retorna o índice do LOD para a distância dada, aplicando histerese em relação ao LOD atual.
    // currentLod = -1 significa "sem LOD atual" (primeira escolha).
    int SelectLod(float distance, int currentLod) const
    {
        if (!IsValid()) return 0;

        // Sem histerese: escolha direta.
        if (currentLod < 0 || hysteresis <= 0.0f)
        {
            for (std::size_t i = 0; i < distances.size(); i++)
                if (distance < distances[i]) return static_cast<int>(i);
            return static_cast<int>(lods.size()) - 1;
        }

        // Com histerese: só troca se a distância ultrapassar o limiar + margem.
        int lod = currentLod;

        // Pode "subir" de LOD (mais detalhe) se ficou mais perto?
        while (lod > 0 && distance < distances[lod - 1] - hysteresis)
            lod--;

        // Pode "descer" de LOD (menos detalhe) se ficou mais longe?
        while (lod < static_cast<int>(distances.size()) &&
               distance > distances[lod] + hysteresis)
            lod++;

        return lod;
    }
};