#include "net/lan_discovery.hpp"
#include "net/lan_protocol.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace airxonix;
int main(){
    lan::LanDiscovery host,client;std::string err;
    assert(client.startClient(err));
    const auto port=std::uint16_t(29000+(getpid()%1000));
    assert(host.startHost(port,err));
    lan::DiscoveredHost candidate{};
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    bool found=false;
    while(std::chrono::steady_clock::now()<until){
        host.poll();client.poll();
        if(client.takeDiscoveredHost(candidate)){found=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(found && candidate.port==port && !candidate.address.empty());
    // Ignore incompatible versions, malformed beacon and random UDP traffic.
    client.close();host.close();
    assert(client.startClient(err));
    const int fd=socket(AF_INET,SOCK_DGRAM,0);assert(fd>=0);
    sockaddr_in dest{};dest.sin_family=AF_INET;dest.sin_port=htons(std::uint16_t(coop::Port+1));
    assert(inet_pton(AF_INET,"127.0.0.1",&dest.sin_addr)==1);
    std::uint8_t invalid[12]={0x41,0x58,0x4d,0x44,0xff,0xff,0x71,0x40,0x72,0x33,0x38,0x36};
    assert(sendto(fd,invalid,sizeof(invalid),0,reinterpret_cast<sockaddr*>(&dest),sizeof(dest))==12);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    client.poll();assert(!client.takeDiscoveredHost(candidate));
    close(fd);
    std::printf("PASS: UDP LAN beacon, host IP %s, dynamic TCP port %u, bad protocol rejected\n",candidate.address.c_str(),port);
}
