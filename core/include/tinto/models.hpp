#pragma once

#include <string>

#include "tinto/config.hpp"

namespace tinto {

std::string FindAsrModel(Quality quality);
std::string FindVadModel();

}  // namespace tinto
