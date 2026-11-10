#pragma once

#include <string>

// Procura o arquivo na pasta atual e, se nao achar, na pasta do executavel.
// Se nenhum existir, devolve o caminho original (o raylib mostra o aviso de erro).
std::string ResolveAssetPath(const std::string& path);
