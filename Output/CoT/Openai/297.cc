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

    // Set TCP NewReno as default
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP Server on Node 1
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Install TCP client on Node 0
    Address clientAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    OnOffHelper clientHelper("ns3::TcpSocketFactory", clientAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));

    // Install FlowMonitor on all nodes
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output FlowMonitor stats to XML
    flowmon->SerializeToXmlFile("flowmon-tcpnewreno.xml", true, true);

    Simulator::Destroy();
    return 0;
}