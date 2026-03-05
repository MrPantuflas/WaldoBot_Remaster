#include "manguera/chatbot.hpp"

manguera::Chatbot::Chatbot(dpp::cluster& bot, std::string host) : m_bot(bot), m_host(std::move(host)) {
    std::cout << "[Chatbot] Created chatbot service\n";
    m_thread = std::thread(&Chatbot::run_queue, this);
}

void manguera::Chatbot::run_queue() {
    std::cout << "[Chatbot] Chatbot thread started\n";

    while (true) {
        std::unique_lock<std::mutex> lock(m_mu);
        m_cv.wait(lock, [this] { return !m_message_queue.empty(); });
        // Placeholder for chatbot processing logic
        std::this_thread::sleep_for(std::chrono::seconds(1));

        lock.unlock();

        manguera::chat_message_t msg = m_message_queue.front();
        m_message_queue.pop();
    }
}