#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set TCP variant to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    // Install devices and channel
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install BulkSendApplication on node 0 (sender)
    uint16_t port = 9;
    BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                            InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer senderApp = bulkSend.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Install PacketSink on node 1 (receiver)
    PacketSinkHelper packetSink("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("p2p-tcp-newreno");

    // Set up NetAnim
    AnimationInterface anim("p2p-tcp-newreno.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 50.0, 20.0);

    // Enable packet metadata for NetAnim
    AnimationInterface::SetPacketMetadata(true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}