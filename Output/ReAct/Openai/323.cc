#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channels
    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup TCP server (PacketSink) on node 1 (index 1), port 8080
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Setup TCP client (OnOffApplication) on node 0 (index 0)
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
    onoff.SetAttribute("DataRate", StringValue("10Mbps")); // DataRate controls max rate, not per-packet
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("MaxBytes", UintegerValue(1024 * 1000));

    // Set app to be always "on" (packet interval will be controlled by MaxBytes and DataRate)
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Optionally, enable flow monitor or ASCII tracing

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}