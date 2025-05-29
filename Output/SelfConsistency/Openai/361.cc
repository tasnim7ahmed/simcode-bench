#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for BulkSendApplication and PacketSink if needed
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on both nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup server (PacketSink)
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Setup client (OnOffApplication for periodic TCP packets)
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps")); // This is ignored for TCP, but set for completeness
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Typical segment size
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); // Always on
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.1))); // send every 0.1s

#if NS_MAJOR_VERSION >= 3 && NS_MINOR_VERSION >= 39
    // OnOffApplication in ns-3.39+ supports the "Interval" attribute for pacing
    // Otherwise, we would need another means to send at intervals
#endif

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}