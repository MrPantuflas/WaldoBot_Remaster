#pragma once
#include <dpp/dpp.h>
#include "services.hpp"

namespace waldo {
  struct Context {
    //dpp::cluster& bot;
    const dpp::slashcommand_t& event;
    Services& services;
  };
}
