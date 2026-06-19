// frize/core/router.hpp ― 명령 라우터 (콕핏 '딸깍' → 디바이스 동작)
#pragma once

#include <map>
#include <vector>
#include <tuple>
#include "frize/schemas.hpp"
#include "frize/bus.hpp"
#include "frize/core/safety.hpp"

namespace frize::core {

struct TrackedCommand {
    Command cmd;
    CommandStatus status{CommandStatus::Queued};
    std::string verdict_reason;
    std::vector<std::tuple<double, CommandStatus, std::string>> history;

    void set(CommandStatus s, const std::string& detail = "") {
        status = s;
        history.emplace_back(now_s(), s, detail);
    }
    json to_json() const {
        json j;
        j["cmd_id"] = cmd.cmd_id;
        j["status"] = status;
        j["reason"] = verdict_reason;
        j["requires_confirm"] = cmd.requires_confirm;
        j["target_device"] = cmd.target_device;
        j["action"] = cmd.action;
        return j;
    }
};

class CommandRouter {
public:
    CommandRouter(MessageBus& bus, SafetyInterlock& interlock)
        : bus_(bus), interlock_(interlock) {}

    // 콕핏에서 들어온 명령 처리. 반환 status로 콕핏 UI 갱신.
    TrackedCommand submit(Command cmd);
    // 지휘관 2차 확인 → 재평가 후 발행(휴먼 인 더 루프)
    std::optional<TrackedCommand> confirm(const std::string& cmd_id, const std::string& by);
    void on_ack(const CommandAck& ack);
    std::vector<TrackedCommand> list_active() const;

private:
    void dispatch(TrackedCommand& t);
    MessageBus& bus_;
    SafetyInterlock& interlock_;
    std::map<std::string, TrackedCommand> commands_;
};

}  // namespace frize::core
