#ifndef PACKETMANAGERH
#define PACKETMANAGERH

#define FS_PORT 8022
#define PACK_LEN 8


#include "socklib.h"
#include "helper.h"

#include <string>
#include <iostream>


class FindSpotPacketManager
{
public:

    PIN_SOCKET clientfd = INVALID_SOCKET;
    bool extended_dbg = false;

    explicit FindSpotPacketManager(PIN_SOCKET sock = INVALID_SOCKET) : clientfd(sock)
    {
        init();
    }
    ~FindSpotPacketManager()
    {
        if(clientfd != INVALID_SOCKET)
            pin_closesocket(clientfd);
        clientfd = INVALID_SOCKET;
    }

    std::string recv_cmd_block()
    {
        char buf[PACK_LEN+1]{};

        int received = pin_recv(clientfd, (char*)&buf, PACK_LEN);
        if(PACK_LEN != received)
        {
            std::cout << "packet len error r = " << received << std::endl;
            return "";
        }

        const size_t packetlen = fromhex(buf);

        std::string r;
        r.resize(packetlen);
        received = pin_recv(clientfd, &r[0], packetlen);
        if(received != packetlen)
        {
            std::cout << "packet RECV error r = " << received << " vs. " << packetlen << " -> " << r << std::endl;
            return "";
        }

        if(extended_dbg)
            std::cout << "packet received: " << r << std::endl;
        return r;
    }

    int send_cmd(const std::string& cmd)
    {
        char num[32]{};
        int tmp = sprintf(num, "%08lX", (unsigned int)cmd.length());
        assertm(tmp == PACK_LEN, "send_cmd error formatting length");
        const std::string packet = num + cmd;

        if(extended_dbg)
            std::cout << "sending packet: " << packet << std::endl;

        size_t len = packet.length();
        int sent = 0;
        while(len > 0)
        {
            int r = pin_send(clientfd, packet.data() + sent, len);
            if(r <= 0)
                break;
            sent += r;
            len -= r;
        }

        return sent;
    }

    PIN_SOCKET block_accept(int port)
    {
        struct pin_sockaddr_in serverAddr, cliAddr;
        PIN_SOCKET sockfd = pin_socket(PF_INET, SOCK_STREAM, 0);

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = pin_htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        pin_bind(sockfd, (struct pin_sockaddr*)&serverAddr, sizeof(serverAddr));

        pin_socklen_t addr_size = sizeof(cliAddr);
        PIN_SOCKET clientfd = INVALID_SOCKET;
        if (pin_listen(sockfd, 5) == 0)
            clientfd = pin_accept(sockfd, (struct pin_sockaddr*)&cliAddr, &addr_size); // accept the connection
        
        return clientfd;
    }

    PIN_SOCKET try_connect(int port)
    {
        PIN_SOCKET sockfd = pin_socket(AF_INET, SOCK_STREAM, 0);
        struct pin_sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = pin_htons(port);
        serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");// INADDR_ANY;

        int ret = pin_connect(sockfd, (struct pin_sockaddr*)&serverAddress, sizeof(serverAddress));
        //int err = p_WSAGetLastError();
        return sockfd;
    }
};


#endif

