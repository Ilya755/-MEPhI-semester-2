#include <string>
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <iostream>
#include <memory>

class Data {
public:
    virtual ~Data() = default;
    virtual std::string GetString() const = 0;
};

class NodeInt : public Data {
public:
    NodeInt(const std::string& value_)
        : value(value_)
        {}

    std::string GetValue() const {
        return value;
    }

    std::string GetString() const override {
        return "i" + value + "e";
    }

private:
    std::string value;
};

class NodeString : public Data {
public:
    NodeString(const std::string& value_)
        : value(value_)
        {}

    std::string GetValue() const {
        return value;
    }

    std::string GetString() const override {
        return std::to_string(value.size()) + ":" + value;
    }

private:
    std::string value;
};

class NodeList : public Data {
public:
    void PushBack(std::shared_ptr<Data> value) {
        values.push_back(value);
    }

    std::string GetString() const override {
        std::string res = "";
        for (auto& val : values) {
            res += val->GetString();
        }
        return "l" + res + "e";
    }

private:
    std::list<std::shared_ptr<Data>> values;
};

class NodeDict : public Data {
public:
    std::shared_ptr<Data> GetKeyValue(const std::string& key) {
        if(data.find(key) == data.end()) {
            throw std::runtime_error("Dict hasn't thith key!");
        }
        else {
            return data[key];
        }
    }

    std::string GetString() const override {
        std::string res = "";
        for (auto& [fir, sec] : data) {
            std::string str_fir = std::to_string(fir.size()) + ":" + fir;
            res += NodeString(fir).GetString() + sec->GetString();
        }
        return "d" + res + "e";
    }

    void SetValue(std::string& key, std::shared_ptr<Data> value) {
        data[key] = value;
    }

private:
    std::map<std::string, std::shared_ptr<Data>> data;
};

std::shared_ptr<Data> Parse(size_t& cur_pos, size_t len, std::string& data) {
    char type = data[cur_pos];
    std::shared_ptr<Data> result;
    if (type == 'i') {
        ++cur_pos;
        std::string value = "";
        while (cur_pos < len && data[cur_pos] != 'e') {
            value += data[cur_pos++];
        }
        ++cur_pos;
        result = std::make_shared<NodeInt>(NodeInt(value));
        return result;
    }
    else if (type >= '0' && type <= '9') {  
        std::string len_str = "";
        while (cur_pos < len && data[cur_pos] != ':') {
            len_str += data[cur_pos++];
        }
        ++cur_pos;
        if (cur_pos >= len) {
            throw std::runtime_error("Index more than data length!");
        }
        size_t cur_len = std::stoul(len_str);
        std::string value = "";
        while (cur_len) {
            value += data[cur_pos++];
            --cur_len;
        }
        if (cur_pos >= len) {
            throw std::runtime_error("Index more than data length!");
        }
        result = std::make_shared<NodeString>(NodeString(value));
        return result;
    }
    else if (type == 'l') {
        result = std::make_shared<NodeList>();
        ++cur_pos;
        while (cur_pos < len && data[cur_pos] != 'e') {
            std::dynamic_pointer_cast<NodeList>(result)->PushBack(Parse(cur_pos, len, data));
        }
        ++cur_pos;
        return result;
    }
    else {
        result = std::make_shared<NodeDict>();
        ++cur_pos;
        while (cur_pos < len && data[cur_pos] != 'e') {
            std::string key = std::dynamic_pointer_cast<NodeString>(Parse(cur_pos, len, data))->GetValue();
            std::dynamic_pointer_cast<NodeDict>(result)->SetValue(key, Parse(cur_pos, len, data));
        }
        ++cur_pos;
        return result;
    }
}

std::vector<Peer> ParsePeers(const std::string& peers) {
    std::vector<Peer> res;
    for (size_t i = 0; i < peers.size(); i += 6) {
        std::string ip = "";
        for (size_t j = 0; j < 4; ++j) {
            ip += std::to_string((unsigned int) static_cast<unsigned char>(peers[i + j]));
            if (j < 3) {
                ip += ".";
            }
        }
        int port = ((unsigned int) static_cast<unsigned char>(peers[i + 4]) << 8) 
                    + (unsigned int) static_cast<unsigned char>(peers[i + 5]);
        res.push_back({ip, port});
    }
    return res;
}

std::vector<std::string> GetPieceHashes(const std::shared_ptr<NodeDict>& dict) {
    std::string data = std::dynamic_pointer_cast<NodeString>(
        std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))
        ->GetKeyValue("pieces"))->GetValue();

    std::vector<std::string> result;
    size_t piece_size = 20;
    size_t len = data.size();
    for (size_t i = 0; i < len; i += piece_size) {
        result.push_back(data.substr(i, 20));
    }
    return result;
}

std::string GetInfoHash(const std::shared_ptr<NodeDict>& dict) {
    std::string data = std::dynamic_pointer_cast<NodeDict>(dict->GetKeyValue("info"))->GetString();
    
    unsigned char info_hash[20];
    SHA1((const unsigned char *) data.c_str(), data.size(), info_hash);
    std::string result = "";
    for (size_t i = 0; i < 20; ++i) {
        result += info_hash[i];
    }
    return result;
}
