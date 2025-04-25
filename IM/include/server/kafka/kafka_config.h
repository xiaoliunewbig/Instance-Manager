#pragma once

#include <string>

namespace im {
namespace kafka {

struct KafkaConfig {
    std::string bootstrap_servers;
    std::string security_protocol;
    int message_timeout_ms;
    bool enable_idempotence;
    int max_in_flight;
};

} // namespace kafka
} // namespace im