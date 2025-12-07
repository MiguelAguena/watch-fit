#pragma once
#include "domain/driven_ports/network.hpp"
#include "BlePeripheralRoutines.hpp"

#include <cstdint>
#include <cstddef>

class BleNetworkPort : driven_ports::network::INodeNetworkPort {
    using NodeId = uint32_t;

    private:
        BlePeripheralRoutines blePeripheralRoutines;

    public:
        BleNetworkPort() :
        blePeripheralRoutines() {}

        ~BleNetworkPort() override {}

        bool connect_to_parent_node() override {
            return true;
        }

        bool send_data_to_parent(const uint8_t* data, std::size_t size) override {
            bool success = true;
            for (std::size_t i = 0; i < size; ++i) {
                success = blePeripheralRoutines.send_messaging_indication(data[i]);
                if(!success) {
                    return false;
                }
            }

            return success;
        }

        NodeId wait_for_child_node_connection() override {
            NodeId r = 0;
            return r;
        }
};