#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (PacketSink) on node 1
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(5.0));

    // Set up TCP client (OnOffApplication) on node 0
    OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(5.0)));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}