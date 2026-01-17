#pragma once

#include <dpp/dpp.h>

namespace policarpo {

enum join_result_e {
    JOINING,
    JOINABLE,
    NOT_IN_VC,
    ALREADY_IN_VC,
    NO_VC,
    VC_DENIED,
    LEAVING
};

join_result_e check_permissions(dpp::guild* g, const dpp::snowflake& user_id, dpp::discord_client* shard);

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild Guild the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* User isn't in vc
 */
std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice(dpp::guild* guild, const dpp::snowflake& user_id);

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild_id Guild Id the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* Unknown guild or user isn't in vc
 */
std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice_from_guild_id(const dpp::snowflake& guild_id, const dpp::snowflake& user_id);

bool has_listener(std::map<dpp::snowflake, dpp::voicestate>* vstate_map);

bool has_permissions(dpp::guild* guild, dpp::user* user, dpp::channel* channel, const std::vector<uint64_t>& permissions);

bool has_permissions_from_ids(const dpp::snowflake& guild_id, const dpp::snowflake& user_id, const dpp::snowflake& channel_id, const std::vector<uint64_t>& permissions);

join_result_e join_voice(const dpp::cluster& bot, const dpp::snowflake& guild_id, const dpp::snowflake& user_id, const dpp::snowflake& shard_id);

join_result_e leave_voice(dpp::discord_client& shard, const dpp::snowflake& guild_id);

}