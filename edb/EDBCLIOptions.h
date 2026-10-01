#pragma once

#include "EDBInterface.h"

#include <CLI/CLI.hpp>

#include <deque>
#include <vector>

void addFileOption(CLI::App& app, std::vector<flashImg>& images,
                   std::deque<std::string>& imageNames);
