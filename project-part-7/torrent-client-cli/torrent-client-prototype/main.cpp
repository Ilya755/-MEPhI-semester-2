#include "torrent_tracker.h"
#include "piece_storage.h"
#include "peer_connect.h"
#include "byte_tools.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <random>
#include <thread>
#include <algorithm>

namespace fs = std::filesystem;

std::mutex cerrMutex, coutMutex;

std::atomic<int> peerCount = 0;

std::string RandomString(size_t length) {
    std::random_device random;
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(random() % ('Z' - 'A') + 'A');
    }
    return result;
}

const std::string PeerId = "TESTAPPDONTWORRY" + RandomString(4);
size_t PiecesToDownload = 20;

void CheckDownloadedPiecesIntegrity(const std::filesystem::path& outputFilename, const TorrentFile& tf, PieceStorage& pieces) {
    pieces.CloseOutputFile();

    if (std::filesystem::file_size(outputFilename) != tf.length) {
        throw std::runtime_error("Output file has wrong size");
    }

    if (pieces.GetPiecesSavedToDiscIndices().size() != pieces.PiecesSavedToDiscCount()) {
        throw std::runtime_error("Cannot determine real amount of saved pieces");
    }

    if (pieces.PiecesSavedToDiscCount() < PiecesToDownload) {
        throw std::runtime_error("Downloaded pieces amount is not enough");
    }

    if (pieces.TotalPiecesCount() != tf.pieceHashes.size() || pieces.TotalPiecesCount() < 200) {
        throw std::runtime_error("Wrong amount of pieces");
    }

    std::vector<size_t> pieceIndices = pieces.GetPiecesSavedToDiscIndices();
    std::sort(pieceIndices.begin(), pieceIndices.end());

    std::ifstream file(outputFilename, std::ios_base::binary);
    for (size_t pieceIndex : pieceIndices) {
        const std::streamoff positionInFile = pieceIndex * tf.pieceLength;
        file.seekg(positionInFile);
        if (!file.good()) {
            throw std::runtime_error("Cannot read from file");
        }
        std::string pieceDataFromFile(tf.pieceLength, '\0');
        file.read(pieceDataFromFile.data(), tf.pieceLength);
        const size_t readBytesCount = file.gcount();
        pieceDataFromFile.resize(readBytesCount);
        const std::string realHash = CalculateSHA1(pieceDataFromFile);

        if (realHash != tf.pieceHashes[pieceIndex]) {
            std::cerr << "File piece with index " << pieceIndex << " started at position " << positionInFile <<
                      " with length " << pieceDataFromFile.length() << " has wrong hash " << HexEncode(realHash) <<
                      ". Expected hash is " << HexEncode(tf.pieceHashes[pieceIndex]) << std::endl;
            throw std::runtime_error("Wrong piece hash");
        }
    }
}

void DeleteDownloadedFile(const std::filesystem::path& outputFilename) {
    std::filesystem::remove(outputFilename);
}

//std::filesystem::path PrepareDownloadDirectory(const std::string& randomString) {
//    std::filesystem::path outputDirectory = "/tmp/downloads";
//    outputDirectory /=  randomString;
//    std::filesystem::create_directories(outputDirectory);
//    return outputDirectory;
//}

bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrentFile, const std::string& ourId, const TorrentTracker& tracker) {
    using namespace std::chrono_literals;

    std::vector<std::thread> peerThreads;
    std::vector<std::shared_ptr<PeerConnect>> peerConnections;

    for (const Peer& peer : tracker.GetPeers()) {
        peerConnections.emplace_back(std::make_shared<PeerConnect>(peer, torrentFile, ourId, pieces, peerCount));
    }

    peerThreads.reserve(peerConnections.size());

    for (auto& peerConnectPtr : peerConnections) {
        peerThreads.emplace_back(
                [peerConnectPtr] () {
                    bool tryAgain = true;
                    int attempts = 0;
                    do {
                        try {
                            ++attempts;
                            peerConnectPtr->Run();
                        } catch (const std::runtime_error& e) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Runtime error: " << e.what() << std::endl;
                        } catch (const std::exception& e) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Exception: " << e.what() << std::endl;
                        } catch (...) {
                            std::lock_guard<std::mutex> cerrLock(cerrMutex);
                            std::cerr << "Unknown error" << std::endl;
                        }
                        tryAgain = peerConnectPtr->Failed() && attempts < 3;
                    } while (tryAgain);
                }
        );
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Started " << peerThreads.size() << " threads for peers" << std::endl;
    }
    std::this_thread::sleep_for(10s);
    while (pieces.PiecesSavedToDiscCount() < PiecesToDownload) {
        if (pieces.PiecesInProgressCount() == 0) {
            {
                std::lock_guard<std::mutex> coutLock(coutMutex);
                std::cout
                        << "Want to download more pieces but all peer connections are not working. Let's request new peers"
                        << std::endl;
            }

            for (auto& peerConnectPtr : peerConnections) {
                peerConnectPtr->Terminate();
            }
            for (std::thread& thread : peerThreads) {
                thread.join();
            }
            return true;
        }
        std::this_thread::sleep_for(1s);
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Terminating all peer connections" << std::endl;
    }
    for (auto& peerConnectPtr : peerConnections) {
        peerConnectPtr->Terminate();
    }

    for (std::thread& thread : peerThreads) {
        thread.join();
    }

    return false;
}

void DownloadTorrentFile(const TorrentFile& torrentFile, PieceStorage& pieces, const std::string& ourId) {
    std::cout << "Connecting to tracker " << torrentFile.announce << std::endl;
    TorrentTracker tracker(torrentFile.announce);
    bool requestMorePeers = false;
    do {
        tracker.UpdatePeers(torrentFile, ourId, 12345);

        if (tracker.GetPeers().empty()) {
            std::cerr << "No peers found. Cannot download a file" << std::endl;
        }

        std::cout << "Found " << tracker.GetPeers().size() << " peers" << std::endl;
        for (const Peer& peer : tracker.GetPeers()) {
            std::cout << "Found peer " << peer.ip << ":" << peer.port << std::endl;
        }

        requestMorePeers = RunDownloadMultithread(pieces, torrentFile, ourId, tracker);
    } while (requestMorePeers);
}

void TestTorrentFile(const fs::path& file, int percent_to_download, const fs::path& outputDirectory) {
    std::cout << "Test torrent file " << std::endl;
    TorrentFile torrentFile;
    try {
        torrentFile = LoadTorrentFile(file);
        PiecesToDownload = ((torrentFile.length / torrentFile.pieceLength) * percent_to_download) / 100;
        std::cout << "Loaded torrent file " << file << ". " << torrentFile.comment << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        return;
    }

    PieceStorage pieces(torrentFile, outputDirectory);

    DownloadTorrentFile(torrentFile, pieces, PeerId);
    std::cout << "Downloaded " << pieces.PiecesSavedToDiscCount() << " pieces" << std::endl;

    CheckDownloadedPiecesIntegrity(outputDirectory / torrentFile.name, torrentFile, pieces);
    std::cout << "Pieces integrity checked" << std::endl;
//    DeleteDownloadedFile(outputDirectory / torrentFile.name);
}

int main(int args_count, char* args[]) {
    if (args_count != 6) {
        std::cerr << "Bad count of input arguments!" << std::endl;
        return 1;
    }

    std::string directory = args[0];
    std::string path_to_save;
    int percent_to_download;
    std::string path_to_torrent;

    for (int i = 1; i < args_count; ++i) {
        std::string arg = args[i];
        if (arg == "-d" && i == 1) {
            path_to_save = args[++i];
        }
        else if (arg == "-p" && i == 3) {
            try {
                percent_to_download = std::stoi(args[4]);
                if (percent_to_download <= 0 || percent_to_download > 100) {
                    throw std::out_of_range("Bad argument, it must be a number between 1 and 100!");
                }
            }
            catch (const std::invalid_argument& e) {
                std::cerr << "Bad argument, it must be a number!" << std::endl;
                return 1;
            }
            catch (const std::out_of_range& e) {
                std::cerr << e.what() << std::endl;
                return 1;
            }
        }
        else {
            path_to_torrent = arg;
        }
    }

    TestTorrentFile(path_to_torrent, percent_to_download, path_to_save);

    return 0;
}
