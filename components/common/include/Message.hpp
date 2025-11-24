#pragma once

enum class MessageType {
    String
};

struct Message {
    MessageType type;
    void* data;
};