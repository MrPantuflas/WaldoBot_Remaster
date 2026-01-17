#pragma once

#include "waldo/command_registry.hpp"

namespace waldo::modules {
  void register_ping(waldo::CommandRegistry& reg);
}