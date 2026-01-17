#include "waldo/modules/music_module.hpp"
#include "waldo/command_registry.hpp"
#include "policarpo/voice_session.hpp"
#include "policarpo/manager.hpp"

// /play query:string
void waldo::modules::register_music(waldo::CommandRegistry& reg) {

  // /play
  dpp::slashcommand join;
  join.set_name("join")
      .set_description("Hace que el bot se una a tu canal de voz.");

  reg.add({
    join,
    [](waldo::Context& ctx) {
      ctx.services.dj->join(ctx.event.command.guild_id, ctx.event);
    }
  });

  dpp::slashcommand play;
  play.set_name("play")
      .set_description("Pone una rola")
      .add_option(dpp::command_option(dpp::co_string, "query", "Nombre de la canción o URL", true));

  reg.add({
    play,
    [](waldo::Context& ctx) {
      auto guild_id = ctx.event.command.guild_id;
      auto value = ctx.event.get_parameter("query");
      std::string query;
      if (std::holds_alternative<std::string>(value)) {
        query = std::get<std::string>(value);
      } 
      // delegate to Policarpo
      ctx.services.dj->play(guild_id, std::move(query), ctx.event);
    }
  });

  // /skip
  dpp::slashcommand skip;
  skip.set_name("skip").set_description("Salta la rola actual.");

  reg.add({
    skip,
    [](waldo::Context& ctx) {
      ctx.services.dj->skip(ctx.event.command.guild_id, ctx.event);
    }
  });

  // /pause
  dpp::slashcommand pause;
  pause.set_name("pause").set_description("Pausa la reproducción.");

  reg.add({
    pause,
    [](waldo::Context& ctx) {
      ctx.services.dj->pause(ctx.event.command.guild_id, ctx.event);
    }
  });

  // stop
  dpp::slashcommand stop;
  stop.set_name("stop").set_description("Detiene la reproducción.");

  reg.add({
    stop,
    [](waldo::Context& ctx) {
      ctx.services.dj->stop(ctx.event.command.guild_id, ctx.event);
    }
  });

  // list
  dpp::slashcommand list;
  list.set_name("list").set_description("Muestra la cola de reproducción.");

  reg.add({
    list,
    [](waldo::Context& ctx) {
      ctx.services.dj->queue(ctx.event.command.guild_id,  ctx.event);
    }
  });

  // leave
  dpp::slashcommand leave;
  leave.set_name("leave").set_description("Hace que el bot salga del canal de voz actual.");

  reg.add({
    leave,
    [](waldo::Context& ctx) {
      ctx.services.dj->leave(ctx.event.command.guild_id, ctx.event);
    }
  });

  // remove
  dpp::slashcommand remove;
  remove.set_name("remove")
      .set_description("Elimina una canción de la cola de reproducción.")
      .add_option(dpp::command_option(dpp::co_integer, "index", "Índice de la canción a eliminar", true));

  reg.add({
    remove,
    [](waldo::Context& ctx) {
      auto guild_id = ctx.event.command.guild_id;
      auto value = ctx.event.get_parameter("index");
      int index = 0;
      if (std::holds_alternative<int64_t>(value)) {
        index = static_cast<int>(std::get<int64_t>(value));
      }
      ctx.services.dj->remove(guild_id, index, ctx.event);
    }
  });

  // jump
  dpp::slashcommand jump;
  jump.set_name("jump")
      .set_description("Salta a una canción específica en la cola de reproducción.")
      .add_option(dpp::command_option(dpp::co_integer, "index", "Índice de la canción a la que saltar.", true));
  reg.add({
    jump,
    [](waldo::Context& ctx) {
      auto guild_id = ctx.event.command.guild_id;
      auto value = ctx.event.get_parameter("index");
      int index = 0;
      if (std::holds_alternative<int64_t>(value)) {
        index = static_cast<int>(std::get<int64_t>(value));
      }

      ctx.services.dj->jump(guild_id, index, ctx.event);
    }
  });

  // set_loop_mode
  dpp::slashcommand set_loop_mode;
  set_loop_mode.set_name("loop")
      .set_description("Establece el modo de repetición para la reproducción de música.")
      .add_option(dpp::command_option(dpp::co_string, "mode", "Modo de repetición", true)
        .add_choice(dpp::command_option_choice("off", "off"))
        .add_choice(dpp::command_option_choice("once", "once"))
        .add_choice(dpp::command_option_choice("current", "current"))
        .add_choice(dpp::command_option_choice("all", "all"))
      );

  reg.add({
    set_loop_mode,
    [](waldo::Context& ctx) {
      auto guild_id = ctx.event.command.guild_id;
      auto value = ctx.event.get_parameter("mode");
      std::string mode;
      if (std::holds_alternative<std::string>(value)) {
        mode = std::get<std::string>(value);
      }
      ctx.services.dj->set_loop_mode(guild_id, mode, ctx.event);
    }
  });
}