#pragma once

#include "PlatformConfig.h"
#include "VectorX.h"

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>   // include before windows.h to avoid winsock.h
#include <ws2tcpip.h>   // for socklen_t
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#ifndef _WIN32
#define SOCKET int
#define closesocket close
#endif
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <cstdint>
#include <memory>
#include <vector>

class Trinity
{
    static const uint16_t TRINLOAD_UDP_PORT = 0xedb0;

public:
    Trinity();
    ~Trinity();

    static std::unique_ptr<Trinity> Open();
    const VectorX<std::string> devices() const;

    void cls();
    void select_record(int record);
    Data read_sector(int cyl, int head, int sector);
    Data read_track(int cyl, int head);
    void send_file(const void* pv_, int len, int start_addr, int exec_addr);

private:
    int Send(const void* pv, int len);
    int Recv(void* pv, int len);

private:
    SOCKET m_socket = static_cast<SOCKET>(-1);
    sockaddr_in m_addr_from{};
    sockaddr_in m_addr_to{};
    VectorX<std::string> m_devices{};
};
