#pragma once

#include <iosfwd>
#include <string_view>

int PrintHelp(std::ostream& out, std::string_view command = {});
