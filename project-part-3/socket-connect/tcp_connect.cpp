#include "tcp_connect.h"
#include "byte_tools.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <limits>
#include <utility>

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds readTimeout)
        : ip_(ip)
        , port_(port)
        , connectTimeout_(connectTimeout)
        , readTimeout_(readTimeout)
        {}

TcpConnect::~TcpConnect() {
    if (sock_status != 2) {
        CloseConnection();
    }
}

void TcpConnect::EstablishConnection() {
    this->sock_ = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_ == -1) {
        throw std::runtime_error("Error in socket!");
    }

    struct sockaddr_in address;

    memset(&address, 0, sizeof(address));

    address.sin_family = PF_INET;
    address.sin_addr.s_addr = inet_addr(ip_.c_str());
    address.sin_port = htons(port_);

    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Error fcntl get flags!");
    }

    if (fcntl(sock_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("Error fcntl set flags!");
    }

    if (connect(sock_, (sockaddr*)&address, sizeof(address)) == -1) {
        struct pollfd fds;
        fds.fd = sock_;
        fds.events = POLLOUT;
        int ready = poll(&fds, 1, static_cast<int>(connectTimeout_.count()));
        if (ready == -1) {
            throw std::runtime_error("Error in poll!");
        }
        else if (!ready) {
            CloseConnection();
            std::cerr << "Error waiting time: " << strerror(errno) << std::endl;
            throw std::runtime_error("Error with time of waiting!");
        }
        else {
            flags = fcntl(sock_, F_GETFL, 0);
            if (flags == -1) {
                throw std::runtime_error("Error fcntl get flags!");
            }
            if (fcntl(sock_, F_SETFL, flags & ~O_NONBLOCK) == -1) {
                throw std::runtime_error("Error fcntl set flags!");
            }
            sock_status = 1;
        }
    }
    else {
        flags = fcntl(sock_, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("Error fcntl get flags!");
        }
        if (fcntl(sock_, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            throw std::runtime_error("Error fcntl set flags!");
        }
        sock_status = 1;
    }
}

void TcpConnect::SendData(const std::string& data) const {
    // if (sock_status == 1) {
        // struct pollfd fds;
        // fds.fd = sock_;
        // fds.events = POLLOUT;
        // int ready = poll(&fds, 1, static_cast<int>(connectTimeout_.count()));
        // if (ready == -1) {
        //     throw std::runtime_error("Error in poll!");
        // }
        // else if (!ready) {
        //     throw std::runtime_error("Descriptor isn't ready!");
        // }
        // else {
            const char* buf = data.c_str();
            size_t len = data.size();
            while (len > 0) {
                ssize_t bytes_sent = send(sock_, buf, len, 0);
                if (bytes_sent == -1) {
                    std::cerr << "Error sending data: " << strerror(errno) << std::endl;
                    throw std::runtime_error("Error send data!");
                }
                buf += bytes_sent;
                len -= bytes_sent;
            }
        // }
    // }
    // else {
    //     throw std::runtime_error("Socket isn't open!");
    // }
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    size_t sz;

    struct pollfd fds;
    fds.fd = sock_;
    fds.events = POLLIN;
    int ready = poll(&fds, 1, static_cast<int>(readTimeout_.count()));
    if (ready == -1) {
        throw std::runtime_error("Error in poll!");
    }
    else if (!ready) {
        throw std::runtime_error("Descriptor isn't ready!");
    }
    else {
        if (!bufferSize) {
            char len[4];
            ssize_t bytes_received = recv(sock_, len, 4, 0);
            if (bytes_received == -1) {
                throw std::runtime_error("Error in recv!");
            }
            else if (!bytes_received) {
                return "";
            }
            else if (bytes_received < 4) {
                throw std::runtime_error("Incorrect buffer size!");
            }
            sz = BytesToInt(len);
        }
        else {
            sz = bufferSize;
        }
        std::string data(sz, 0);
        char* buf = &data[0];
        while (sz > 0) {
            ssize_t bytes_received = recv(sock_, buf, sz, 0);
            if (bytes_received == -1) {
                throw std::runtime_error("Error in recv!");
            }
            else if (!bytes_received) {
                return "";
            }
            buf += bytes_received;
            sz -= bytes_received;
        }
        return data;
    }
}

void TcpConnect::CloseConnection() {
    close(sock_);
    sock_status = 2;
}

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}

