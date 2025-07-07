#include "piece_storage.h"
#include <iostream>

PieceStorage::PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory)
        : tf_(tf)
        , outputDirectory_(outputDirectory)
        , outputFile_(outputDirectory_ / tf_.name, std::ios::binary | std::ios::out) {
            std::unique_lock<std::shared_mutex> lock(sh_mutex_);
            outputFile_.seekp(tf_.length - 1);
            outputFile_.write("\0", 1);
            for (size_t i = 0; i < tf.length / tf.pieceLength; ++i) {
                if (i < tf.length / tf.pieceLength - 1) {
                    remainPieces_.push(std::make_shared<Piece>(Piece(i, tf.pieceLength, tf.pieceHashes[i])));
                }
                else {
                    remainPieces_.push(std::make_shared<Piece>(Piece(i, tf.length % tf.pieceLength, tf.pieceHashes[i])));
                }
            };
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::unique_lock<std::shared_mutex> lock(sh_mutex_);
    if (!remainPieces_.empty()) {
        PiecePtr next_piece = remainPieces_.front();
        remainPieces_.pop();
        return next_piece;
    }
    else {
        return nullptr;
    }
}

void PieceStorage::AddPiece(const PiecePtr& piece) {
    std::unique_lock<std::shared_mutex> lock(sh_mutex_);
    remainPieces_.push(piece);
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    std::unique_lock<std::shared_mutex> lock(sh_mutex_);
    if (piece->HashMatches()) {
        SavePieceToDisk(piece);
    }
    else {
        piece->Reset();
        remainPieces_.push(piece);
    }
}

bool PieceStorage::QueueIsEmpty() const {
    std::shared_lock<std::shared_mutex> lock(sh_mutex_);
    return remainPieces_.empty();
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::shared_lock<std::shared_mutex> lock(sh_mutex_);
    return piecesSavedToDisc_.size();
}

size_t PieceStorage::TotalPiecesCount() const {
    std::shared_lock<std::shared_mutex> lock(sh_mutex_);
    return tf_.length / tf_.pieceLength + (tf_.length % tf_.pieceLength == 0 ? 0 : 1);
}

void PieceStorage::CloseOutputFile() {
    std::unique_lock<std::shared_mutex> lock(sh_mutex_);
    if (outputFile_.is_open()) {
        outputFile_.close();
    }
    else {
        throw std::runtime_error("OutputFile was closed early!");
    }
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() const {
    std::shared_lock<std::shared_mutex>lock(sh_mutex_);
    return piecesSavedToDisc_;
}

size_t PieceStorage::PiecesInProgressCount() const {
    std::shared_lock<std::shared_mutex> lock(sh_mutex_);
    return (tf_.length / tf_.pieceLength + (tf_.length % tf_.pieceLength == 0 ? 0 : 1)) - piecesSavedToDisc_.size() - remainPieces_.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    std::unique_lock<std::shared_mutex> lock(save_mutex_);
    if (!outputFile_.is_open()) {
        throw std::runtime_error("OutputFile is not open!");
    }
    else {
        if(setOfPiecesSavedToDisc_.find(piece->GetIndex()) != setOfPiecesSavedToDisc_.end()) {
            throw std::runtime_error("Piece already saved to disk!");
        }
        else {
            outputFile_.seekp(piece->GetIndex() * tf_.pieceLength);
            outputFile_.write(piece->GetData().data(), piece->GetData().size());
            if (outputFile_.good()) {
                piecesSavedToDisc_.push_back(piece->GetIndex());
                setOfPiecesSavedToDisc_.insert(piece->GetIndex());
            }
            else {
                throw std::runtime_error("Error while saving piece to disk!");
            }
        }
    }
}
