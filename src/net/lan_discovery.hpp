#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace airxonix::lan {

enum class DiscoveryRole { None, Host, Client };

struct DiscoveredHost {
    std::string address;
    std::uint16_t port=0;
};

class LanDiscovery {
public:
    LanDiscovery()=default;
    ~LanDiscovery();
    LanDiscovery(const LanDiscovery&)=delete;
    LanDiscovery& operator=(const LanDiscovery&)=delete;

    bool startHost(std::uint16_t tcpPort,std::string& error);
    bool startClient(std::string& error);
    void poll();
    bool takeDiscoveredHost(DiscoveredHost& out);
    void close();
    DiscoveryRole role() const{return role_;}
    std::string error() const{return error_;}
private:
    int fd_=-1;
    DiscoveryRole role_=DiscoveryRole::None;
    std::uint16_t tcpPort_=0;
    std::chrono::steady_clock::time_point lastBroadcastAt_{};
    bool discoveredReady_=false;
    DiscoveredHost discovered_{};
    std::string error_;
};

} // namespace airxonix::lan
