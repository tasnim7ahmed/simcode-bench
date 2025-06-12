#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: central node and two peripheral nodes
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    // Connect each peripheral node to the central node using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices0 = p2p.Install(centralNode.Get(0), peripheralNodes.Get(0));
    NetDeviceContainer devices1 = p2p.Install(centralNode.Get(0), peripheralNodes.Get(1));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Set up a packet sink (UDP receiver) on the second peripheral node
    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSink("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSink.Install(peripheralNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up OnOff application to generate traffic from first peripheral node to second
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(interfaces1.GetAddress(1), sinkPort));
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(peripheralNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}