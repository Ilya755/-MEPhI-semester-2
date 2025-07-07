#include "message.h"
#include "byte_tools.h"

Message Message::Parse(const std::string &messageString) {
    return {static_cast<MessageId>(messageString[0]), messageString.length(), messageString.substr(1, messageString.length() - 1)};
}

Message Message::Init(MessageId id, const std::string& payload) {
    return {id, payload.length() + 1, payload};
}

std::string Message::ToString() const {
    std::string send_message = IntToBytes(messageLength) +
                               static_cast<char>(id) + payload;
    return send_message;
}

