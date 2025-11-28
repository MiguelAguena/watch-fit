#pragma once

#include <vector>

#include "domain/use_case.hpp"
#include "domain/driven_ports/network.hpp"
#include "domain/driven_ports/time.hpp"


namespace domain::nodes {

    constexpr std::size_t MAX_CHILD_NODES = 5;


    class NodeConnectorCivilian : IRootUseCase {
    public:
        struct CompositionSt {
            driven_ports::time::ITimePort& time_port;
            driven_ports::network::INodeNetworkPort& network_port;
        };

        NodeConnectorCivilian(CompositionSt& composition)
            : time_port(composition.time_port), network_port(composition.network_port) {
        }

        void run() override {
            while (!network_port.connect_to_parent_node()) {
                time_port.sleep_for(500); // Sleep for half a second before retrying to avoid busy-waiting
            }
            while (true) {
                driven_ports::network::NodeId child_id = network_port.wait_for_child_node_connection();
                child_nodes.push_back(child_id);
            }
        }

        std::vector<driven_ports::network::NodeId> get_child_nodes() const {
            return child_nodes;
        }

    private:
        driven_ports::network::INodeNetworkPort& network_port;
        driven_ports::time::ITimePort& time_port;

        std::vector<driven_ports::network::NodeId> child_nodes;
    };


};