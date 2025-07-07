#include "piece_storage.h"
#include <iostream>

PieceStorage::PieceStorage(const TorrentFile& tf)
    : tf_(tf) {
        for (size_t i = 0; i < tf.length / tf.pieceLength; ++i) {
            if (i < tf.length / tf.pieceLength - 1) {
                remainPieces_.push(std::make_shared<Piece>(Piece(i, tf.pieceLength, tf.pieceHashes[i])));
            }
            else {
                remainPieces_.push(std::make_shared<Piece>(Piece(i, tf.length % tf.pieceLength, tf.pieceHashes[i])));
            }
        }
}


PiecePtr PieceStorage::GetNextPieceToDownload() {
//    std::lock_guard<std::mutex> lock(mutex_);
    if (!QueueIsEmpty()) {
        PiecePtr next_piece = remainPieces_.front();
        remainPieces_.pop();
        return next_piece;
    }
    else {
        return nullptr;
    }
}

void PieceStorage::AddPiece(const PiecePtr& piece) {
//    std::lock_guard<std::mutex> lock(mutex_);
    remainPieces_.push(piece);
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
//    std::lock_guard<std::mutex> lock(mutex_);
    if (piece->HashMatches()) {
        SavePieceToDisk(piece);
        while (!remainPieces_.empty()) {
            remainPieces_.pop();
        }
    }
    else {
        piece->Reset();
    }

}

bool PieceStorage::QueueIsEmpty() const {
//    std::lock_guard<std::mutex> lock(mutex_);
    return remainPieces_.empty();
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
//    std::lock_guard<std::mutex> lock(mutex_);
    return piecesSavedToDisc_.size();
}

size_t PieceStorage::TotalPiecesCount() const {
//    std::lock_guard<std::mutex> lock(mutex_);
    return tf_.length / tf_.pieceLength;
}

void PieceStorage::CloseOutputFile() {
//    std::lock_guard<std::mutex> lock(mutex_);
    if (outputFile_.is_open()) {
        outputFile_.close();
    }
    else {
        throw std::runtime_error("OutputFile was closed early!");
    }
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() const {
//    std::lock_guard<std::mutex> lock(mutex_);
    return piecesSavedToDisc_;
}

size_t PieceStorage::PiecesInProgressCount() const {
//    std::lock_guard<std::mutex> lock(mutex_);
    return remainPieces_.size();
}

//void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
//    if (!outputFile_.is_open()) {
//        throw std::runtime_error("OutputFile is not open!");
//    }
//    else {
//        if(setOfPiecesSavedToDisc_.find(piece->GetIndex()) != setOfPiecesSavedToDisc_.end()) {
//            throw std::runtime_error("Piece already saved to disk!");
//        }
//        else {
//            outputFile_.seekp(piece->GetIndex() * tf_.pieceLength);
//            outputFile_.write(piece->GetData().data(), piece->GetData().size());
//            piecesSavedToDisc_.push_back(piece->GetIndex());
//            setOfPiecesSavedToDisc_.insert(piece->GetIndex());
//        }
//    }
//}

void PieceStorage::SavePieceToDisk(PiecePtr piece) {
    // Эта функция будет переопределена при запуске вашего решения в проверяющей системе
    // Вместо сохранения на диск там пока что будет проверка, что часть файла скачалась правильно
    std::cout << "Downloaded piece " << piece->GetIndex() << std::endl;
    std::cerr << "Clear pieces list, don't want to download all of them" << std::endl;
    while (!remainPieces_.empty()) {
        remainPieces_.pop();
    }
}
