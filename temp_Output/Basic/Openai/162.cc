#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRingNetAnimExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create ring links: 0-1-2-3-0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer dev30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to each link
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iface23 = address.Assign(dev23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iface30 = address.Assign(dev30);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP server on node 2
    uint16_t servPort = 50000;
    Address servAddress(InetSocketAddress(iface12.GetAddress(1), servPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", servAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(30.0));

    // TCP client on node 0
    // Set up BulkSendApplication for a persistent flow
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", servAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(30.0));

    // Enable packet logging
    Config::Connect("/NodeList/*/DeviceList/*/TxQueue/Enqueue",
                    MakeCallback([](Ptr<const QueueItem> item) {
                        NS_LOG_INFO("Packet enqueued: " << *item->GetPacket());
                    }));

    // NetAnim setup
    AnimationInterface anim("tcp-ring-netanim.xml");

    anim.SetConstantPosition(nodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 150.0, 50.0);
    anim.SetConstantPosition(nodes.Get(2), 150.0, 150.0);
    anim.SetConstantPosition(nodes.Get(3), 50.0, 150.0);

    anim.EnablePacketMetadata(true);

    // Start simulation
    Simulator::Stop(Seconds(32.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}