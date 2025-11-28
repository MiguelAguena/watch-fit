#pragma once
#include <cstdint>
#include <cstddef>


namespace driven_ports::network {

    using NodeId = uint32_t;

    class INodeNetworkPort {
    public:
        virtual ~INodeNetworkPort() = default;

        // Sends a request to find and connect to a parent node in the network.
        virtual bool connect_to_parent_node() = 0;

        // Sends data over to the parent node.
        virtual bool send_data_to_parent(const uint8_t* data, std::size_t size) = 0;

        // Waits for and accepts a connection from a child node, returning its NodeId.
        virtual NodeId wait_for_child_node_connection() = 0;

    };
}