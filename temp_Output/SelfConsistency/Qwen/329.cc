#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpThreeNodesSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect Node 0 to Node 2
    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));

    // Connect Node 1 to Node 2
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP server on Node 2
    uint16_t port = 9; // Well-known echo port number
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client on Node 0
    Address sinkAddress(InetSocketAddress(interfaces02.GetAddress(1), port));
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer clientApp0 = clientHelper.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    // Set up TCP client on Node 1
    Address sinkAddress1(InetSocketAddress(interfaces12.GetAddress(1), port));
    BulkSendHelper clientHelper1("ns3::TcpSocketFactory", sinkAddress1);
    clientHelper1.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer clientApp1 = clientHelper1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    // Set up PCAP tracing
    pointToPoint.EnablePcapAll("tcp-three-nodes-simulation");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}