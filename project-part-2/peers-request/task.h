#pragma once

#include "peer.h"
#include "torrent_file.h"
#include "bencode_parser.h"
#include <string>
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <cpr/cpr.h>

class TorrentTracker {
public:
    /*
     * url - адрес трекера, берется из поля announce в .torrent-файле
     */
    TorrentTracker(const std::string& url)
        : url_(url)
        {}

    /*
     * Получить список пиров у трекера и сохранить его для дальнейшей работы.
     * Запрос пиров происходит посредством HTTP GET запроса, данные передаются в формате bencode.
     * Такой же формат использовался в .torrent файле.
     * Подсказка: посмотрите, что было написано в main.cpp в домашнем задании torrent-file
     *
     * tf: структура с разобранными данными из .torrent файла из предыдущего домашнего задания.
     * peerId: id, под которым представляется наш клиент.
     * port: порт, на котором наш клиент будет слушать входящие соединения (пока что мы не слушаем и на этот порт никто
     *  не сможет подключиться).
     */
    void UpdatePeers(const TorrentFile& tf, std::string peerId, int port) {
        cpr::Response res = cpr::Get(
                cpr::Url{url_},
                cpr::Parameters {
                        {"info_hash", tf.infoHash},
                        {"peer_id", peerId},
                        {"port", std::to_string(port)},
                        {"uploaded", std::to_string(0)},
                        {"downloaded", std::to_string(0)},
                        {"left", std::to_string(tf.length)},
                        {"compact", std::to_string(1)}
                },
                cpr::Timeout{20000}
        );
        

        if (res.status_code != 200) {
            std::cerr << "Failed connection with status_code: " << res.status_code << std::endl;
            return;
        }

        if (res.text.find("failure reason") != std::string::npos) {
            std::cerr << "Server responded '" << res.text << "'" << std::endl;
            return;
        }

        size_t cur_pos = 0;
        size_t res_text_len = res.text.size();
        std::string data = res.text;
        std::shared_ptr<NodeDict> dict = std::dynamic_pointer_cast<NodeDict>(Parse(cur_pos, res_text_len, data));
        std::string peers = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("peers"))->GetValue();
        this->peers_ = ParsePeers(peers);
    }

    /*
     * Отдает полученный ранее список пиров
     */
    const std::vector<Peer>& GetPeers() const {
        return peers_;
    }

private:
    std::string url_;
    std::vector<Peer> peers_;
};

TorrentFile LoadTorrentFile(const std::string& filename) {
    // Перенесите сюда функцию загрузки файла из предыдущего домашнего задания
    std::ifstream read_file(filename, std::ios::binary);
    if (!read_file.is_open()) {
        perror("read_file");
    }

    std::stringstream buffer;
    buffer << read_file.rdbuf();
    std::string data = buffer.str();

    TorrentFile tf;

    size_t cur_pos = 0;
    size_t len = data.size();

    std::shared_ptr<NodeDict> dict = std::dynamic_pointer_cast<NodeDict>(Parse(cur_pos, len, data));

    tf.announce = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("announce"))->GetValue();
    tf.comment = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("comment"))->GetValue();
    tf.pieceLength = std::stoul(std::dynamic_pointer_cast<NodeInt>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("piece length"))->GetValue());
    tf.length = std::stoul(std::dynamic_pointer_cast<NodeInt>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("length"))->GetValue());
    tf.name = std::dynamic_pointer_cast<NodeString>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("name"))->GetValue();

    tf.pieceHashes = GetPieceHashes(dict);

    tf.infoHash = GetInfoHash(dict);

    read_file.close();

    return tf;
}
