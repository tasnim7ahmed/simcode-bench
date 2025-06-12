#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link between the two nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (PacketSink) on node 1
    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(5.0));

    // Set up TCP client on node 0
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(5.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp_simulation");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}