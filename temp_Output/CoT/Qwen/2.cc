#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Disable logging for simplicity
    LogComponentDisableAll(LOG_LEVEL_ALL);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point channel with 5 Mbps data rate and 2 ms propagation delay
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install the point-to-point devices on both nodes and connect them
    NetDeviceContainer devices = p2p.Install(nodes);

    // Verify that the link is successfully set up by checking device count
    if (devices.GetN() == 2) {
        std::cout << "Point-to-point link setup successful between two nodes." << std::endl;
    } else {
        std::cerr << "Error: Point-to-point link setup failed." << std::endl;
        return 1;
    }

    // Run the simulation (no traffic or protocols, just setup)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}