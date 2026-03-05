#pragma once
#include <dpp/dpp.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>
#include <queue>

namespace manguera {

struct chat_message_t {
    dpp::snowflake channel_id;
    dpp::snowflake user_id;
    std::string& content;
};

class Chatbot {
public:
    explicit Chatbot(dpp::cluster& bot, std::string host = "") : m_bot(bot), m_host(std::move(host)) {}
    void run_queue();

private:
    dpp::cluster& m_bot;
    std::string m_host;
    std::thread m_thread;
    std::mutex m_mu;
    std::condition_variable m_cv;
    std::queue<chat_message_t> m_message_queue;
};
}