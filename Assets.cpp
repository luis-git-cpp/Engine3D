#include "Assets.h"

#include "raylib.h"

std::string ResolveAssetPath(const std::string& path)
{
    if (FileExists(path.c_str())) return path;

    const std::string besideExe = std::string(GetApplicationDirectory()) + path;
    if (FileExists(besideExe.c_str())) return besideExe;

    return path;
}
