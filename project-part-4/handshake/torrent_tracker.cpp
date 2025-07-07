#include "torrent_tracker.h"
#include "bencode.h"
#include "byte_tools.h"
#include <cpr/cpr.h>
#include <cstring>
#include <iostream>

TorrentTracker::TorrentTracker(const std::string& url)
        : url_(url)
{}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port) {
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
    std::shared_ptr<NodeDict> dict = std::dynamic_pointer_cast<NodeDict>(Bencode::Parse(cur_pos, res_text_len, data));
    std::string peers = std::dynamic_pointer_cast<NodeString>(dict->GetKeyValue("peers"))->GetValue();
    this->peers_ = Bencode::ParsePeers(peers);
}

const std::vector<Peer> &TorrentTracker::GetPeers() const {
    return peers_;
}
