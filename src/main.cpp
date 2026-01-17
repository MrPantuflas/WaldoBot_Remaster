#include <dpp/dpp.h>
#include "dotenv.hpp"
#include <filesystem>
#include "waldo/services.hpp"
#include "waldo/command_registry.hpp"
#include "waldo/modules/music_module.hpp"
#include "policarpo/track_index.hpp"
#include "policarpo/manager.hpp"
#include "policarpo/song_manager.hpp"

int main() {
  DotenvError result = Dotenv::load(".env");
  if (result != DotenvError::Success) {
      std::cerr << "Error: " << Dotenv::getLastError() << "\n";
      return 1;
  }

  policarpo::track_cache_init();

  const std::string token = Dotenv::get("BOT_TOKEN");

  if (token.empty()) {
    std::cerr << "Error: BOT_TOKEN is not set in the environment variables.\n";
    return 1;
  }

  dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);
  bot.on_log(dpp::utility::cout_logger());

  waldo::Services services{};
  services.dj = std::make_shared<policarpo::Manager>(bot);

  std::filesystem::create_directory("songs");

  waldo::CommandRegistry reg;
  waldo::modules::register_music(reg);
  bool dev_mode;

  #ifdef DEBUG_MODE
  std::cout << "Waldo is starting in " <<  "development" << " mode.\n";
  dev_mode = true;
  #else
  std::cout << "Waldo is starting in production mode.\n";
  dev_mode = false;
  #endif

  dpp::snowflake guild_id = dev_mode ? dpp::snowflake(Dotenv::get("GUILD_ID")) : dpp::snowflake{0};

  bot.on_ready([&](const dpp::ready_t&) {
    if (!dpp::run_once<struct register_cmds>()) return;

    if (guild_id) {
      reg.register_guild(bot, guild_id);   // fast propagation
      std::cout << "Registering commands for guild ID: " << guild_id << "\n";
    } else {
      reg.register_global(bot);   // slow propagation
      std::cout << "Registering global commands.\n";
    }      

  });

  bot.on_slashcommand([&](const dpp::slashcommand_t& event) {
    reg.dispatch(/*bot,*/ event, services);
  });

  bot.on_voice_track_marker([&](const dpp::voice_track_marker_t& event) {
    // ev.voice_client is the voice client that crossed a marker
    services.dj->on_voice_track_marker(event);
  });

  bot.on_voice_client_disconnect([&](const dpp::voice_client_disconnect_t& event) {
    // ev.voice_client is the voice client that got disconnected
    services.dj->on_voice_client_disconnect(event);
  });

  bot.on_voice_ready([&](const dpp::voice_ready_t& event) {
    //ev.voice_client is the voice client that is ready
    services.dj->on_voice_ready(event);
  });

  bot.on_voice_state_update([&](const dpp::voice_state_update_t& event) {
    // Handle voice state changes (including disconnections)
    services.dj->on_voice_state_update(event);
  });


  bot.start(dpp::st_wait);
}
