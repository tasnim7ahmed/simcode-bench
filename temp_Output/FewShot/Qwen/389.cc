#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging for debugging
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create two nodes (Sender and Receiver)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from 10.1.1.0/24 range
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP receiver (Node 1) on port 9
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP sender (Node 0) sending 1024-byte packets every millisecond
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1000000"))); // Rate high enough to send every ms
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}