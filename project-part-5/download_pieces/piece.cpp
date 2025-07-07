#include "byte_tools.h"
#include "piece.h"
#include <iostream>
#include <algorithm>

namespace {
constexpr size_t BLOCK_SIZE = 1 << 14;
}

Piece::Piece(size_t index, size_t length, std::string hash) 
    : index_(index)
    , length_(length)
    , hash_(hash)
    , blocks_(length / BLOCK_SIZE + (length % BLOCK_SIZE == 0 ? 0 : 1)) {
        for (size_t i = 0; i < blocks_.size(); ++i) {
            blocks_[i].piece = static_cast<uint32_t>(index);
            blocks_[i].offset = static_cast<uint32_t>(BLOCK_SIZE * i);
            if (i < blocks_.size() - 1) {
                blocks_[i].length = static_cast<uint32_t>(BLOCK_SIZE);
            }
            else {
                blocks_[i].length = static_cast<uint32_t>(length % BLOCK_SIZE == 0 ? BLOCK_SIZE : length % BLOCK_SIZE);
            }
            blocks_[i].status = Block::Status::Missing;
        }
    }

bool Piece::HashMatches() const {
    return GetDataHash() == GetHash();
}

Block* Piece::FirstMissingBlock() {
    auto it = blocks_.begin();
    for (; it != blocks_.end(); ++it) {
        if ((*it).status == Block::Status::Missing) {
            return &(*it);
        }
    }
    if (it == blocks_.end()) {
        return nullptr;
    }
}

size_t Piece::GetIndex() const {
    return index_;
}

void Piece::SaveBlock(size_t blockOffset, std::string data) {
    blocks_[blockOffset / BLOCK_SIZE].status = Block::Status::Retrieved;
    blocks_[blockOffset / BLOCK_SIZE].data = data;
}

bool Piece::AllBlocksRetrieved() const {
    for (const auto& block : blocks_) {
        if (block.status != Block::Status::Retrieved) {
            return false;
        }
    }
    return true;
}

std::string Piece::GetData() const {
    std::string download_data = "";
    for (const auto& block : blocks_) {
        download_data += block.data;
    }
    return download_data;
}

std::string Piece::GetDataHash() const {
    return CalculateSHA1(GetData());
}

const std::string& Piece::GetHash() const {
    return hash_;
}

void Piece::Reset() {
    for (auto& block : blocks_) {
        block.status = Block::Status::Missing;
        block.data.clear();
    }
}