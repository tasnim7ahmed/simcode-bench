#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

void
TxCallback(Ptr<const Packet> packet)
{
    NS_LOG_INFO("Packet transmitted at " << Simulator::Now().GetSeconds() << "s, size=" << packet->GetSize());
}

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_INFO("Packet received at " << Simulator::Now().GetSeconds() << "s, size=" << packet->GetSize());
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("StarTopologySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 0-central, 1-peripheral, 2-peripheral
    NodeContainer nodes;
    nodes.Create(3);

    // Connect node 0 to node 1, and node 0 to node 2
    NodeContainer n0n1(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2(nodes.Get(0), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    uint16_t port = 4000;
    // Install UDP server on node 2
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    
    // Connect receive callback for logging
    serverApps.Get(0)->GetObject<UdpServer>()->TraceConnectWithoutContext(
        "Rx", MakeCallback(&RxCallback));

    // Install UDP client on node 1, destination = node 2
    UdpClientHelper client(i0i2.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Connect transmit callback for logging
    clientApps.Get(0)->TraceConnectWithoutContext(
        "Tx", MakeCallback(&TxCallback));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}