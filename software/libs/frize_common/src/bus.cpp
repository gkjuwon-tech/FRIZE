// frize/bus.cpp ― MessageBus 구현 (Paho MQTT C++)
#include "frize/bus.hpp"
#include "frize/log.hpp"

#include <mqtt/async_client.h>
#include <mqtt/callback.h>

namespace frize {

static auto LOG = make_logger("bus");

bool topic_matches(const std::string& filter, const std::string& topic) {
    auto split = [](const std::string& s) {
        std::vector<std::string> out; size_t start = 0, pos;
        while ((pos = s.find('/', start)) != std::string::npos) { out.push_back(s.substr(start, pos-start)); start = pos+1; }
        out.push_back(s.substr(start)); return out;
    };
    auto f = split(filter); auto t = split(topic);
    for (size_t i = 0; i < f.size(); ++i) {
        if (f[i] == "#") return true;
        if (i >= t.size()) return false;
        if (f[i] == "+") continue;
        if (f[i] != t[i]) return false;
    }
    return f.size() == t.size();
}

struct MessageBus::Impl : public virtual mqtt::callback {
    std::string client_id, host, password, username;
    int port, keepalive_s;
    std::unique_ptr<mqtt::async_client> client;
    mqtt::connect_options conn_opts;
    std::vector<std::pair<std::string, BusHandler>> handlers;
    std::mutex mtx;
    std::atomic<bool> is_connected{false};
    bool has_will{false};

    Impl(std::string cid, std::string h, int p, std::string u, std::string pw, int ka)
        : client_id(std::move(cid)), host(std::move(h)), password(std::move(pw)),
          username(std::move(u)), port(p), keepalive_s(ka) {
        const std::string uri = "tcp://" + host + ":" + std::to_string(port);
        client = std::make_unique<mqtt::async_client>(uri, client_id);
        client->set_callback(*this);

        conn_opts.set_clean_session(true);
        conn_opts.set_keep_alive_interval(keepalive_s);
        conn_opts.set_automatic_reconnect(1, 30);  // 1s ~ 30s 백오프
        if (!username.empty()) conn_opts.set_user_name(username);
        if (!password.empty()) conn_opts.set_password(password);
    }

    // ----- mqtt::callback -----
    void connected(const std::string& /*cause*/) override {
        is_connected = true;
        LOG->info("bus connected: {}@{}:{}", client_id, host, port);
        std::lock_guard<std::mutex> lk(mtx);
        for (auto& [filter, _] : handlers) {
            try { client->subscribe(filter, 1); }
            catch (const std::exception& e) { LOG->error("subscribe {} 실패: {}", filter, e.what()); }
        }
    }
    void connection_lost(const std::string& cause) override {
        is_connected = false;
        LOG->warn("bus disconnected: {} (자동 재연결 시도)", cause.empty() ? "unknown" : cause);
    }
    void message_arrived(mqtt::const_message_ptr msg) override {
        const std::string topic = msg->get_topic();
        Envelope env;
        try { env = Envelope::from_string(msg->to_string()); }
        catch (const std::exception& e) { LOG->error("bad envelope on {}: {}", topic, e.what()); return; }

        std::vector<BusHandler> to_call;
        {
            std::lock_guard<std::mutex> lk(mtx);
            for (auto& [filter, h] : handlers)
                if (topic_matches(filter, topic)) to_call.push_back(h);
        }
        for (auto& h : to_call) {
            try { h(topic, env); }
            catch (const std::exception& e) { LOG->error("handler error on {}: {}", topic, e.what()); }
        }
    }
};

MessageBus::MessageBus(std::string client_id, std::string host, int port,
                       std::string username, std::string password, int keepalive_s)
    : p_(std::make_unique<Impl>(std::move(client_id), std::move(host), port,
                                std::move(username), std::move(password), keepalive_s)) {}

MessageBus::~MessageBus() { try { stop(); } catch (...) {} }

void MessageBus::set_device_will(const std::string& device_id, DeviceType type) {
    Heartbeat hb; hb.device_id = device_id; hb.device_type = type; hb.state = DeviceState::Offline;
    auto env = Envelope::wrap(MessageType::Heartbeat, device_id, hb);
    mqtt::will_options will(Topic::heartbeat(device_id), env.to_string(), 1, true);
    p_->conn_opts.set_will(will);
    p_->has_will = true;
}

void MessageBus::subscribe(const std::string& topic_filter, BusHandler handler) {
    std::lock_guard<std::mutex> lk(p_->mtx);
    p_->handlers.emplace_back(topic_filter, std::move(handler));
    if (p_->is_connected) {
        try { p_->client->subscribe(topic_filter, 1); }
        catch (const std::exception& e) { LOG->error("late subscribe {} 실패: {}", topic_filter, e.what()); }
    }
}

void MessageBus::publish(const std::string& topic, const Envelope& env, int qos, bool retain) {
    if (!p_->is_connected) { LOG->warn("publish drop(미연결): {}", topic); return; }
    try {
        auto m = mqtt::make_message(topic, env.to_string(), qos, retain);
        p_->client->publish(m);
    } catch (const std::exception& e) {
        LOG->error("publish {} 실패: {}", topic, e.what());
    }
}

void MessageBus::start() {
    try { p_->client->connect(p_->conn_opts); }
    catch (const std::exception& e) { LOG->error("connect 실패: {}", e.what()); }
}

void MessageBus::stop() {
    if (!p_ || !p_->client) return;
    try { if (p_->client->is_connected()) p_->client->disconnect()->wait(); }
    catch (...) {}
    p_->is_connected = false;
}

bool MessageBus::connected() const { return p_->is_connected.load(); }

}  // namespace frize
