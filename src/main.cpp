#include "../include/dht_bootstrap.hpp"
#include <iostream>

int main() {
    // Generate a random Node ID
    DHT::NodeID my_node_id = DHT::DHTBootstrap::generate_random_node_id();

    // Create a DHTBootstrap instance
    DHT::DHTBootstrap dht_bootstrap(my_node_id);

    // Add bootstrap nodes (example: public BitTorrent DHT nodes)
    // dht_bootstrap.add_bootstrap_node("67.215.246.10", 6881);
    // dht_bootstrap.add_bootstrap_node("router.utorrent.com", 6881);
    // dht_bootstrap.add_bootstrap_node("dht.transmissionbt.com", 6881);

    // // Start the bootstrap process
    // dht_bootstrap.bootstrap();

    // // Keep it running
    // dht_bootstrap.run();

    // Add a well-known bootstrap node
    // dht_bootstrap.add_bootstrap_node("router.bittorrent.com", 6881);
    dht_bootstrap.add_bootstrap_node("67.215.246.10", 6881);

    // Generate a random target Node ID
    DHT::NodeID target_id = DHT::DHTBootstrap::generate_random_node_id();

    // Send a FIND_NODE request
    auto nodes = dht_bootstrap.send_find_node_request(dht_bootstrap.get_bootstrap_nodes()[0], target_id);

    // Print the received nodes
    std::cout << "Received " << nodes.size() << " nodes:" << std::endl;
    for (const auto& node : nodes) {
        std::cout << "  Node: " << node.ip << ":" << node.port  
                  << " (ID: " << DHT::node_id_to_hex(node.id) << ")" << std::endl;
    }
    
    dht_bootstrap.bootstrap();

    // Keep it running
    dht_bootstrap.run();
    return 0;
}

