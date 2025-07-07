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
    if (sock_status == 1) {
        CloseConnection();
    }
}

void TcpConnect::EstablishConnection() {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == -1) {
        throw std::runtime_error("Error in socket!");
    }

    struct sockaddr_in address;

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_.c_str());
    address.sin_port = htons(port_);

    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Error fcntl get flags!");
    }

    if (fcntl(sock_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("Error fcntl set flags!");
    }

    if (connect(sock_, (const struct sockaddr*)& address, sizeof(address)) == 0) {
        flags = fcntl(sock_, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("Error fcntl get flags!");
        }
        if (fcntl(sock_, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            throw std::runtime_error("Error fcntl set flags!");
        }
    }
    else {
        struct pollfd fds;
        memset(&fds, 0, sizeof(fds));
        fds.fd = sock_;
        fds.events = POLLOUT;
        int ready = poll(&fds, 1, connectTimeout_.count());
        if (ready == -1) {
            throw std::runtime_error("Error in poll!");
        }
        else if (!ready) {
//            CloseConnection();
//            std::cerr << "Error waiting time: " << strerror(errno) << std::endl;
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
        }
    }
    sock_status = 1;
}

void TcpConnect::SendData(const std::string& data) const {
    if (sock_status == 2 || sock_ == -1) {
        throw ("Socket was closed!");
    }
    const char* buf = data.c_str();
    size_t len = data.size();
    while (len > 0) {
//        std::cout << "ПЕРЕД ОТПРАВКОЙ\n";
        int bytes_sent;
        if (sock_status == 1) {
            bytes_sent = send(sock_, buf, len, );
        }
        else {
            throw std::runtime_error("Socket was closed before the data was sent!");
        }
//        std::cout << "ПОСЛЕ ОТПРАВКИ\n";
        if (bytes_sent == -1) {
//            std::cerr << "Error sending data: " << strerror(errno) << std::endl;
            throw std::runtime_error("Error send data!");
        }
        else if (bytes_sent == 0) {
            throw std::runtime_error("Bad value of bytes_sent!");
        }
        buf += bytes_sent;
        len -= bytes_sent;
    }
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    if (sock_status == 2 || sock_ == -1) {
        throw ("Socket was closed!");
    }
    size_t sz = 4;

    struct pollfd fds;
    memset(&fds, 0, sizeof(fds));
    fds.fd = sock_;
    fds.events = POLLIN;

    if (!bufferSize) {
        std::string len(4, 0);
        char *lenbufptr = &len[0];
        while (sz > 0) {
            int ready = poll(&fds, 1, readTimeout_.count());
            if (ready == -1) {
                throw std::runtime_error("Error in poll!");
            }
            else if (!ready) {
                throw std::runtime_error("Descriptor isn't ready!");
            }
            else {
                ssize_t bytes_received = recv(sock_, lenbufptr, sz, 0);
                if (bytes_received == -1) {
                    throw std::runtime_error("Error in recv!");
                }
                else if (bytes_received == 0) {
                    throw std::runtime_error("Bad value of bytes_received!");
                }
                lenbufptr += bytes_received;
                sz -= bytes_received;
            }
        }
        sz = BytesToInt(len);
    }
    else {
        sz = bufferSize;
    }
    std::string data(sz, 0);
    char* buf = &data[0];
    while (sz > 0) {
        int ready = poll(&fds, 1, readTimeout_.count());
        if (ready == -1) {
            throw std::runtime_error("Error in poll!");
        }
        else if (!ready) {
            throw std::runtime_error("Descriptor isn't ready!");
        }
        else {
            ssize_t bytes_received = recv(sock_, buf, sz, 0);
            if (bytes_received == -1) {
                throw std::runtime_error("Error in recv!");
            } else if (!bytes_received) {
                throw std::runtime_error("Bad value of bytes_received!");
            }
            buf += bytes_received;
            sz -= bytes_received;
        }
    }
    return data;
}

void TcpConnect::CloseConnection() {
    if (sock_status == 1) {
        sock_status = 2;
        close(sock_);
    }
}

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}
