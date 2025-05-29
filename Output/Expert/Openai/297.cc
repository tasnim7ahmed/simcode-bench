#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Set TCP NewReno as default TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices and Channels
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install server application on Node 1
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install client application on Node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Install FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Save FlowMonitor results
    monitor->SerializeToXmlFile("flowmon-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}