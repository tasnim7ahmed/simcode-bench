#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices on nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack (required for printing interface addresses)
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Print interface addresses to confirm connections
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress interfaceAddress = ipv4->GetAddress(1, 0);
        Ipv4Address ipAddress = interfaceAddress.GetLocal();
        std::cout << "Node " << i << " IP Address: " << ipAddress << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}