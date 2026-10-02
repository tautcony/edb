#pragma once

#include "EDBInterface.h"

#include <CLI/CLI.hpp>

#include <vector>

void addFileOption(CLI::App& app, std::vector<flashImg>& images);
