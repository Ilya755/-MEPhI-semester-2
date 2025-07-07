#include "torrent_file.h"
#include "bencode.h"
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <sstream>

TorrentFile LoadTorrentFile(const std::string& filename) {
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

    std::shared_ptr<NodeDict> dict = std::dynamic_pointer_cast<NodeDict>(Bencode::Parse(cur_pos, len, data));

    tf.announce = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("announce"))->GetValue();
    tf.comment = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("comment"))->GetValue();
    tf.pieceLength = std::stoul(std::dynamic_pointer_cast<NodeInt>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("piece length"))->GetValue());
    tf.length = std::stoul(std::dynamic_pointer_cast<NodeInt>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("length"))->GetValue());
    tf.name = std::dynamic_pointer_cast<NodeString>(std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetKeyValue("name"))->GetValue();

    tf.pieceHashes = Bencode::GetPieceHashes(dict);

    tf.infoHash = Bencode::GetInfoHash(dict);

    read_file.close();

    return tf;
}