#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Command line argument parser
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create two nodes: one client, one server
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install a TCP server (PacketSink) on node 1
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Configure client socket for non-persistent connection (BulkSend terminates after sending all bytes)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", serverAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(10240)); // 10 KB transfer (for non-persistent)
    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-non-persistent");

    // NetAnim: assign node positions for visualization
    AnimationInterface anim("tcp-non-persistent.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 90.0, 50.0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}