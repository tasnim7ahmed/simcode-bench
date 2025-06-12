#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer centralNode;
    centralNode.Create(1); // Node 0

    NodeContainer branchNodes;
    branchNodes.Create(3); // Nodes 1, 2, 3

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    Ipv4InterfaceContainer interfaces[3];

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(branchNodes);

    Ipv4AddressHelper address;
    for (uint8_t i = 0; i < 3; ++i) {
        devices[i] = p2p.Install(centralNode.Get(0), branchNodes.Get(i));
        address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Install applications
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint8_t i = 0; i < 3; ++i) {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(branchNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing
    p2p.EnablePcapAll("branch_topology_udp_traffic");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}