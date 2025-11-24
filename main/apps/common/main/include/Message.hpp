enum class MessageType {
    ValueMessage
};

struct Message {
    MessageType type;
    void* data;
};