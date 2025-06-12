#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between adjacent nodes to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t j = (i + 1) % 4;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up echo server applications on each node, listening on port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps[4];
    for (uint32_t i = 0; i < 4; ++i) {
        serverApps[i] = echoServer.Install(nodes.Get(i));
        serverApps[i].Start(Seconds(1.0));
        serverApps[i].Stop(Seconds(10.0));
    }

    // Set up echo client applications sending packets to the next node in the ring
    UdpEchoClientHelper echoClient(interfaces[1].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t destNode = (i + 1) % 4;
        Ipv4Address destAddr = interfaces[destNode].GetAddress(0);
        UdpEchoClientHelper client(destAddr, 9);
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        clientApps[i] = client.Install(nodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    // Enable packet capture for analysis
    p2p.EnablePcapAll("ring_topology");

    // Enable NetAnim XML trace file
    AnimationInterface anim("ring_topology.xml");
    for (uint32_t i = 0; i < 4; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0); // Red color
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}