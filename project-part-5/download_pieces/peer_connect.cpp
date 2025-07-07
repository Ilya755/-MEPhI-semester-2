#include "byte_tools.h"
#include "peer_connect.h"
#include "message.h"
#include <iostream>
#include <sstream>
#include <utility>

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

PeerConnect::PeerConnect(const Peer& peer, const TorrentFile &tf, std::string selfPeerId, PieceStorage& pieceStorage)
        : tf_(tf)
        , socket_(TcpConnect(peer.ip, peer.port, 500ms, 500ms))
        , selfPeerId_(selfPeerId)
        , piecesAvailability_(PeerPiecesAvailability())
        , terminated_(false)
        , choked_(true)
        , pieceStorage_(pieceStorage)
        , pendingBlock_(false)
        , failed_(false)
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

void PeerConnect::Terminate() {
    std::cerr << "Terminate" << std::endl;
    terminated_ = true;
}

bool PeerConnect::Failed() const {
    return failed_;
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

void PeerConnect::RequestPiece() {
    if (!terminated_) {
        if (pieceInProgress_ == nullptr) {
            PiecePtr next_piece = pieceStorage_.GetNextPieceToDownload();
//            while ((next_piece == nullptr ? false : true) &&
//                   !piecesAvailability_.IsPieceAvailable(next_piece->GetIndex())) {
//                        pieceStorage_.AddPiece(next_piece);
//                        next_piece = pieceStorage_.GetNextPieceToDownload();
//
//            }
            if (next_piece != nullptr && piecesAvailability_.IsPieceAvailable(next_piece->GetIndex())) {
                pieceInProgress_ = next_piece;
            }
            else {
                return;
            }
        }
        if (!pendingBlock_) {
            pendingBlock_ = true;
            Block* cur_block = pieceInProgress_->FirstMissingBlock();
            std::string request = IntToBytes(13) + static_cast<char>(MessageId::Request) +
                                  IntToBytes(pieceInProgress_->GetIndex()) +
                                  IntToBytes((*cur_block).offset) + IntToBytes((*cur_block).length);
            socket_.SendData(request);
        }
    }
    else {
        throw std::runtime_error("Peer was terminated!");
    }
}
void PeerConnect::MainLoop() {
    while (!terminated_) {
        std::string receive_data = socket_.ReceiveData(0);
        if (!receive_data.empty()) {
            MessageId id = static_cast<MessageId>((int) receive_data[0]);
            std::string data = receive_data.substr(1, receive_data.size() - 1);
            if (id == MessageId::Have) {
                size_t len = BytesToInt(data.substr(0, 4));
                size_t id = BytesToInt(data.substr(4, 1));
                size_t piece_availability = BytesToInt(data.substr(5, 4));
                piecesAvailability_.SetPieceAvailability(piece_availability);
            }
            else if (id == MessageId::Piece) {
                size_t piece_index = BytesToInt(data.substr(0, 4));
                size_t offset = BytesToInt(data.substr(4, 4));
                std::string blocks = data.substr(8, data.size() - 8);
                if (pieceInProgress_ != nullptr && pieceInProgress_->GetIndex() == piece_index) {
                    pieceInProgress_->SaveBlock(offset, blocks);
                    if (pieceInProgress_->AllBlocksRetrieved()) {
                        pieceStorage_.PieceProcessed(pieceInProgress_);
                        pieceInProgress_ = nullptr;
                        Terminate();
                    }
                }
                pendingBlock_ = false;
            }
            else if (id == MessageId::Choke) {
                choked_ = true;
//                Terminate();
            }
            else if (id == MessageId::Unchoke) {
                choked_ = false;
            }
            else {
                throw std::runtime_error("WRONG MessageId!");
            }
        }

        if (!choked_ && !pendingBlock_) {
            RequestPiece();
        }
    }
}