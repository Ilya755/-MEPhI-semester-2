#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <memory>
#include "peer.h"
#include <openssl/sha.h>

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

namespace Bencode {

/*
 * В это пространство имен рекомендуется вынести функции для работы с данными в формате bencode.
 * Этот формат используется в .torrent файлах и в протоколе общения с трекером
 */

    std::shared_ptr<Data> Parse(size_t& cur_pos, size_t len, std::string& data);
    std::vector<Peer> ParsePeers(const std::string& peers);
    std::vector<std::string> GetPieceHashes(const std::shared_ptr<NodeDict>& dict);
    std::string GetInfoHash(const std::shared_ptr<NodeDict>& dict);
}
