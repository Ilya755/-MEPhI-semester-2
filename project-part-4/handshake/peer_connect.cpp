#include "byte_tools.h"
#include "peer_connect.h"
#include "message.h"
#include <iostream>
#include <sstream>
#include <utility>
#include <cassert>

using namespace std::chrono_literals;

PeerPiecesAvailability::PeerPiecesAvailability()
    {}

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield) 
    : bitfield_(bitfield)
    {}

bool PeerPiecesAvailability::IsPieceAvailable(size_t pieceIndex) const {
    return ((bitfield_[pieceIndex / 8] >> (7 - pieceIndex % 8)) & 1);
}

void PeerPiecesAvailability::SetPieceAvailability(size_t pieceIndex) {
    bitfield_[pieceIndex / 8] |= (1 << (7 - (pieceIndex % 8)));
}

size_t PeerPiecesAvailability::Size() const {
    size_t sz = 0;
    for (auto& ch : bitfield_) {
        sz += __builtin_popcount((int) ch);
    }
    return sz;
}

PeerConnect::PeerConnect(const Peer& peer, const TorrentFile &tf, std::string selfPeerId) 
    : tf_(tf)
    , socket_(TcpConnect(peer.ip, peer.port, 500ms, 500ms))
    , selfPeerId_(selfPeerId)
    , peerId_(peer.ip) 
    , piecesAvailability_(PeerPiecesAvailability())
    , terminated_(false) 
    , choked_(true)
    {}

void PeerConnect::Run() {
    while (!terminated_) {
        if (EstablishConnection()) {
            std::cout << "Connection established to peer" << std::endl;
            MainLoop();
        } else {
            std::cerr << "Cannot establish connection to peer" << std::endl;
            Terminate();
        }
    }
}

void PeerConnect::PerformHandshake() {
    if (!terminated_) {
        socket_.EstablishConnection();
        std::string handshake = "BitTorrent protocol00000000" + tf_.infoHash + selfPeerId_;
        handshake = ((char) 19) + handshake;
        socket_.SendData(handshake);
        std::string peer_handshake = socket_.ReceiveData(68);
        if (peer_handshake[0] != ((char) 19) || 
                peer_handshake.substr(1, 19) != "BitTorrent protocol") {
                    throw std::runtime_error("Bad peer_handshake!");
        }
        if (peer_handshake.substr(28, 20) != tf_.infoHash) {
            throw std::runtime_error("Bad peer_handshake in infoHash!");
        }
    }
    else {
        throw std::runtime_error("Peer was terminated!");
    }
}

bool PeerConnect::EstablishConnection() {
    try {
        PerformHandshake();
        ReceiveBitfield();
        SendInterested();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to establish connection with peer " << socket_.GetIp() << ":" <<
            socket_.GetPort() << " -- " << e.what() << std::endl;
        return false;
    }
}

void PeerConnect::ReceiveBitfield() {
    if (!terminated_) {
        std::string receive_data = socket_.ReceiveData(0);
        if (receive_data[0] == (char) 20) {
            receive_data = socket_.ReceiveData(0);
        }
        MessageId id = static_cast<MessageId>((int) receive_data[0]);
        if (id == MessageId::BitField) {
            piecesAvailability_ = PeerPiecesAvailability(receive_data.substr(1, receive_data.size() - 1));
        }
        else if (id == MessageId::Unchoke) {
            choked_ = false;
        }
        else {
            throw std::runtime_error("Wrong bitfield!");
        }
    }
    else {
        throw std::runtime_error("Peer was terminated!");
    }
}

void PeerConnect::SendInterested() {
    if (!terminated_) {
        std::string interested = IntToBytes(1) + static_cast<char>(MessageId::Interested);
        socket_.SendData(interested);
    }
    else {
        throw std::runtime_error("Peer was terminated!");
    }
}

void PeerConnect::Terminate() {
    std::cerr << "Terminate" << std::endl;
    terminated_ = true;
}

void PeerConnect::MainLoop() {
    /*
     * При проверке вашего решения на сервере этот метод будет переопределен.
     * Если вы провели handshake верно, то в этой функции будет работать обмен данными с пиром
     */
    std::cout << "Dummy main loop" << std::endl;
    Terminate();
}