#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable Tcp logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack and set TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (PacketSink) on node 1
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client (OnOffApplication) on node 0
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("DataRate", StringValue("1Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}