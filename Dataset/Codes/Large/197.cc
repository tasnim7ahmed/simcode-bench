#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PNetworkExample");

int main(int argc, char *argv[])
{
    // Log level settings
    LogComponentEnable("P2PNetworkExample", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the P2P devices and channels between node0-node1 and node1-node2
    NetDeviceContainer devices0 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // Enable routing on the middle node (node1)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create a TCP server on node2 to receive traffic
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces1.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create a TCP client on node0 to send traffic to node2
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
    clientApps.Add(clientHelper.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing (pcap)
    pointToPoint.EnablePcapAll("p2p-network");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output received packets
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
    NS_LOG_INFO("Total Packets Received by Node 2: " << sink1->GetTotalRx());

    Simulator::Destroy();
    return 0;
}

