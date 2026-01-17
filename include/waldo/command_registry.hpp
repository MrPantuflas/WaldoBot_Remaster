#pragma once
#include <dpp/dpp.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "context.hpp"

namespace waldo {
  using HandlerFn = std::function<void(Context&)>;

  struct CommandDef {
    dpp::slashcommand spec;
    HandlerFn handler;
  };

  class CommandRegistry {
   public:
    void add(CommandDef def) {
      std::string name = def.spec.name;
      handlers_[name] = std::move(def.handler);
      defs_.push_back(std::move(def));
    }

    void register_global(dpp::cluster& bot) const {
      std::vector<dpp::slashcommand> commands;
      commands.reserve(defs_.size());
      for (const auto& d : defs_) {
        commands.push_back(d.spec);
      }
      bot.global_bulk_command_create(commands);
    }

    void register_guild(dpp::cluster& bot, dpp::snowflake guild_id) const {
      std::vector<dpp::slashcommand> commands;
      commands.reserve(defs_.size());
      for (const auto& d : defs_) {
        commands.push_back(d.spec);
      }
      bot.guild_bulk_command_create(commands, guild_id);
    }

    void dispatch(/*dpp::cluster& bot, */const dpp::slashcommand_t& event, Services& services) const {
      auto it = handlers_.find(event.command.get_command_name());
      if (it == handlers_.end()) {
        event.reply(dpp::message("Comando desconocido").set_flags(dpp::m_ephemeral));
        return;
      }
      Context ctx{/*bot,*/ event, services};
      it->second(ctx);
    }

   private:
    std::unordered_map<std::string, HandlerFn> handlers_;
    std::vector<CommandDef> defs_;
  };
}
