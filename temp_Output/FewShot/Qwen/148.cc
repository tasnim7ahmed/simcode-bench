#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Central node is node 0
    Ptr<Node> centralNode = nodes.Get(0);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    Ipv4InterfaceContainer interfaces[3];

    InternetStackHelper stack;
    stack.Install(nodes);

    // Install point-to-point links from node 0 to nodes 1, 2, and 3
    for (uint32_t i = 0; i < 3; ++i) {
        NodeContainer pair = NodeContainer(centralNode, nodes.Get(i + 1));
        devices[i] = p2p.Install(pair);

        char subnetBase[16];
        sprintf(subnetBase, "10.%d.1.0", i);  // Unique subnets: 10.0.1.0, 10.1.1.0, 10.2.1.0
        Ipv4AddressHelper address;
        address.SetBase(subnetBase, "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up UDP Echo Server on node 0
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(centralNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients on nodes 1, 2, 3 sending to node 0
    for (uint32_t i = 0; i < 3; ++i) {
        UdpEchoClientHelper client(interfaces[i].GetAddress(1), port); // node 0's IP on this link
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(i + 1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream oss;
        oss << "branch-topology-node0-node" << (i+1);
        p2p.EnablePcapAll(oss.str());
    }

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}