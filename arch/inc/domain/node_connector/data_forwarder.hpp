#pragma once

#include "domain/use_case.hpp"
#include "domain/driven_ports/network.hpp"

namespace domain::nodes {

    class DataForwarder {
    public:
        DataForwarder(driven_ports::network::INodeNetworkPort& network_port)
            : network_port(network_port) {
        }

    private:
        driven_ports::network::INodeNetworkPort& network_port;
    };

};