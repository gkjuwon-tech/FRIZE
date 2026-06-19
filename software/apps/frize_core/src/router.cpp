#include "frize/core/router.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"

namespace frize::core {

static auto LOG = make_logger("router");

TrackedCommand CommandRouter::submit(Command cmd) {
    TrackedCommand t; t.cmd = std::move(cmd); t.status = CommandStatus::Queued;

    Verdict v = interlock_.evaluate(t.cmd);
    t.verdict_reason = v.reason;
    t.cmd.reason = v.reason;

    if (v.decision == Decision::Reject) {
        t.set(CommandStatus::Rejected, v.reason);
        LOG->warn("명령 거부 [{}]: {}", t.cmd.target_device, v.reason);
        commands_[t.cmd.cmd_id] = t;
        return t;
    }
    if (v.decision == Decision::NeedsConfirm && !t.cmd.confirmed) {
        t.cmd.requires_confirm = true;
        t.set(CommandStatus::NeedsConfirm, v.reason);
        LOG->info("명령 보류(확인필요) [{}]: {}", t.cmd.target_device, v.reason);
        commands_[t.cmd.cmd_id] = t;
        return t;
    }
    dispatch(t);
    commands_[t.cmd.cmd_id] = t;
    return t;
}

std::optional<TrackedCommand> CommandRouter::confirm(const std::string& cmd_id, const std::string& by) {
    auto it = commands_.find(cmd_id);
    if (it == commands_.end()) return std::nullopt;
    TrackedCommand& t = it->second;
    if (t.status != CommandStatus::NeedsConfirm) return t;

    t.cmd.confirmed = true;
    t.cmd.params["_confirmed_by"] = by;
    Verdict v = interlock_.evaluate(t.cmd);
    if (v.decision == Decision::Reject) {
        t.set(CommandStatus::Rejected, v.reason);
        return t;
    }
    dispatch(t);
    return t;
}

void CommandRouter::dispatch(TrackedCommand& t) {
    auto env = Envelope::wrap(MessageType::Command, "core", t.cmd);
    bus_.publish(Topic::command(t.cmd.target_device), env, 1);
    t.set(CommandStatus::Sent, "디바이스로 발행");
    LOG->info("명령 발행 [{}→{}] ({})", static_cast<int>(t.cmd.action), t.cmd.target_device, t.cmd.reason);
}

void CommandRouter::on_ack(const CommandAck& ack) {
    auto it = commands_.find(ack.cmd_id);
    if (it == commands_.end()) return;
    it->second.set(ack.status, ack.detail);
}

std::vector<TrackedCommand> CommandRouter::list_active() const {
    std::vector<TrackedCommand> out;
    for (auto& [_, t] : commands_) {
        if (t.status == CommandStatus::Done || t.status == CommandStatus::Failed ||
            t.status == CommandStatus::Rejected) continue;
        out.push_back(t);
    }
    return out;
}

}  // namespace frize::core
