#pragma once
#include <memory>

namespace policarpo { class Manager; }
namespace manguera { class Chatbot; }

namespace waldo {
  struct Services {
    std::shared_ptr<policarpo::Manager> dj;
    std::shared_ptr<manguera::Chatbot> chatbot;
  };
}
