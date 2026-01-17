#include <dpp/dpp.h>
#include "waldo/command_registry.hpp"
#include "waldo/modules/ping_module.hpp"

void waldo::modules::register_ping(waldo::CommandRegistry& reg) {
  dpp::slashcommand ping;
  ping.set_name("ping")
      .set_description("Check if the bot is alive");

  reg.add({
    ping,
    [](waldo::Context& ctx) {
      ctx.event.reply("Pong 🏓");
    }
  });
}