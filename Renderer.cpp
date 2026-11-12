#include "Renderer.h"

void Renderer::SetTint(Color tint)
{
    for (int i = 0; i < model.materialCount; i++)
        model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = tint;
}