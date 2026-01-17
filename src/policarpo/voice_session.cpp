#include "policarpo/voice_session.hpp"
#include <type_traits>

namespace policarpo {

// Simpler compile-time detection using a different SFINAE approach
template<typename T = void>
struct connect_method_detector {
    // Try to call the method with cluster parameter
    template<typename = void>
    static auto test_with_cluster(dpp::guild* g, const dpp::cluster& c, dpp::snowflake u) 
        -> decltype(g->connect_member_voice(c, u, false, false, false), std::true_type{});
    static std::false_type test_with_cluster(...);
    
    // Try to call the method without cluster parameter  
    template<typename = void>
    static auto test_without_cluster(dpp::guild* g, dpp::snowflake u) 
        -> decltype(g->connect_member_voice(u, false, false, false), std::true_type{});
    static std::false_type test_without_cluster(...);
    
    static constexpr bool has_cluster_method = decltype(test_with_cluster(nullptr, std::declval<const dpp::cluster&>(), dpp::snowflake{}))::value;
    static constexpr bool has_simple_method = decltype(test_without_cluster(nullptr, dpp::snowflake{}))::value;
};

// Safe wrapper using runtime conditional instead of constexpr
void safe_connect_member_voice(dpp::guild* guild, const dpp::cluster& bot, const dpp::snowflake& user_id, bool self_mute = false, bool self_deaf = false, bool dave = false) {
    // Just use the modern signature directly since AUR version should be recent
    guild->connect_member_voice(user_id, self_mute, self_deaf, dave);
}

std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice(dpp::guild* guild, const dpp::snowflake& user_id) {
    for (auto &fc : guild->channels) {
        auto channel = dpp::find_channel(fc);
        if (!channel || (!channel->is_voice_channel() && !channel->is_stage_channel())) {
            continue;
        }

        std::map<dpp::snowflake, dpp::voicestate> voice_members = channel->get_voice_members();

        if (voice_members.find(user_id) != voice_members.end()) {
            return { channel, voice_members };
        }
    }
    return { NULL, {} };
}

join_result_e check_permissions(dpp::guild* g, const dpp::snowflake& user_id, dpp::discord_client* shard) {
    
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vc_user, vc_shard;
    vc_user = get_voice(g, user_id);

    if (!vc_user.first) {
        return NOT_IN_VC;
    }

    dpp::user* const sha_user = dpp::find_user(shard->shard_id);
    
    /*uint64_t cperm = g->permission_overwrites(g->base_permissions(sha_user), sha_user, vc_user.first);
    
    if (!(cperm & dpp::p_view_channel && cperm & dpp::p_connect)){
        return VC_DENIED;
    }*/

    vc_shard = get_voice(g, shard->shard_id);
    bool vcclient_cont = vc_shard.first != nullptr;

    if(!vcclient_cont && shard->connecting_voice_channels.find(g->id) != shard->connecting_voice_channels.end()) {
        std::cerr << "[Voice Session] Disconnecting as not in vc but connected state still in cache: " << g->id << "\n";

        shard->disconnect_voice(g->id);
    }

    dpp::voiceconn* v = shard->get_voice(g->id);
    if (vcclient_cont && v && v->channel_id != vc_shard.first->id) {
        vcclient_cont = false;
        std::cerr << "[Voice Session] Disconnecting as it seems I just got moved to a different vc and connection not updated yet: " << g->id << "\n";

        //waldo::manager.set_disconnecting(g->id, vc_shard.first->id);

        shard->disconnect_voice(g->id);
    }

    bool reconnecting = false;

    if (vcclient_cont && vc_shard.first->id != vc_user.first->id) {
        if (policarpo::has_listener(&vc_shard.second)) {
            //out = "❌ Manito ya estoy en un canal de voz.";
            return ALREADY_IN_VC;
        }
    }

    return JOINABLE;
}

std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice_from_guild_id(const dpp::snowflake& guild_id, const dpp::snowflake& user_id) {

    dpp::guild* g = dpp::find_guild(guild_id);
    if (g)
        return get_voice(g, user_id);

    return { NULL, {} };
}

bool has_listener(std::map<dpp::snowflake, dpp::voicestate>* vstate_map) {
    if (!vstate_map || vstate_map->size () <= 1) {
        return false;
    }

    for (const std::pair<const dpp::snowflake, dpp::voicestate>& r : *vstate_map) {
        const dpp::snowflake uid = r.second.user_id;
        if (!uid) {
            continue;
        }

        dpp::user* u = dpp::find_user(uid);

        if (!u || u->is_bot ()) {
            continue;
        }
        return true;
    }

    return false;
}

bool has_permissions(dpp::guild* guild, dpp::user* user, dpp::channel* channel, const std::vector<uint64_t>& permissions) {
    if (!guild || !user || !channel) {
        return false;
    }

    uint64_t p = guild->permission_overwrites(guild->base_permissions(user), user, channel);
    for (uint64_t i : permissions) {
        if (!(p & i)) {
            return false;
        }
    }
    return true;
}

bool has_permissions_from_ids(const dpp::snowflake& guild_id, const dpp::snowflake& user_id, const dpp::snowflake& channel_id, const std::vector<uint64_t>& permissions) {
    if (!guild_id || !user_id || !channel_id) {
        return false;
    }
    return has_permissions(dpp::find_guild(guild_id), dpp::find_user(user_id), dpp::find_channel(channel_id), permissions);
}

join_result_e join_voice(const dpp::cluster& bot, const dpp::snowflake& guild_id, const dpp::snowflake& user_id, const dpp::snowflake& shard_id) {

    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> user_channel_and_vs, bot_channel_and_vs;

    user_channel_and_vs = get_voice_from_guild_id(guild_id, user_id);

    // user isn't in voice channel
    if (!user_channel_and_vs.first)
        return NOT_IN_VC;

    // no channel cached
    if (!user_channel_and_vs.first->id)
        return NO_VC;

    bot_channel_and_vs = get_voice_from_guild_id(guild_id, shard_id);

    // client already in a guild session
    if (has_listener(&bot_channel_and_vs.second)) {
        return ALREADY_IN_VC;
    }
    /*if (!has_permissions_from_ids(guild_id, shard_id, user_channel_and_vs.first->id, { dpp::p_view_channel, dpp::p_connect })) {
        // no permission
        return VC_DENIED;
    }*/

    
    //std::thread t ([&bot, &user_id, &guild_id]() {
        dpp::guild* g = dpp::find_guild(guild_id);
        #ifdef ENABLE_DAVE
            safe_connect_member_voice(g, bot, user_id, false, false, true);
        #else
            safe_connect_member_voice(g, bot, user_id, false, true, false);
        #endif
    //});

    // success dispatching join voice channel
    return JOINING;
}

join_result_e leave_voice(dpp::discord_client& shard, const dpp::snowflake& guild_id) {
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> bot_channel_and_vs;

   /* bot_channel_and_vs = get_voice_from_guild_id(guild_id, shard.shard_id);

    // client not in a guild session
    if (!has_listener(&bot_channel_and_vs.second)) {
        return NOT_IN_VC;
    }*/

    shard.disconnect_voice(guild_id);

    return LEAVING;

}

} // namespace policarpo