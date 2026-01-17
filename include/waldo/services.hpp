#pragma once
#include <memory>

namespace policarpo { class Manager; }

namespace waldo {
  struct Services {
    std::shared_ptr<policarpo::Manager> dj;
  };
}
