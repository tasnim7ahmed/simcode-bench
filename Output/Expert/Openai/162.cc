#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyTcpExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("RingTopologyTcpExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create ring links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    std::vector<NetDeviceContainer> deviceContainers(4);
    std::vector<NodeContainer> ringLinks(4);

    // Connect nodes: 0-1-2-3-0
    for (uint32_t i = 0; i < 4; ++i)
    {
        ringLinks[i] = NodeContainer(nodes.Get(i), nodes.Get((i + 1) % 4));
        deviceContainers[i] = p2p.Install(ringLinks[i]);
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    std::vector<Ipv4InterfaceContainer> interfaces(4);
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(deviceContainers[i]);
    }

    // Enable static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP server on node 2 (attached to interface with node 3 for demonstration)
    uint16_t port = 50000;
    Address sinkAddress(
        InetSocketAddress(interfaces[2].GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // TCP client on node 0, persistent connection (OnOff app with long on time)
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("DataRate", StringValue("2Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=20]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client.SetAttribute("StopTime", TimeValue(Seconds(19.0)));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));

    // Enable PCAP tracing for all links
    for (uint32_t i = 0; i < 4; ++i)
    {
        p2p.EnablePcapAll("ring-topology-tcp", true);
    }

    // NetAnim setup
    AnimationInterface anim("ring-topology-tcp.xml");

    // Set Node positions
    anim.SetConstantPosition(nodes.Get(0), 50.0, 100.0);
    anim.SetConstantPosition(nodes.Get(1), 100.0, 150.0);
    anim.SetConstantPosition(nodes.Get(2), 150.0, 100.0);
    anim.SetConstantPosition(nodes.Get(3), 100.0, 50.0);

    // Enable packet metadata for NetAnim
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}