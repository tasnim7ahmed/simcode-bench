#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes (1 hub + 8 spokes = 9 nodes)
    NodeContainer nodes;
    nodes.Create(9);

    // Create PointToPoint links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect spokes to the hub
    NetDeviceContainer devices;
    for (int i = 1; i < 9; ++i) {
        NetDeviceContainer link = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
        devices.Add(link.Get(0));
        devices.Add(link.Get(1));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create packet sink on the hub (node 0)
    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create OnOff applications on the spokes (nodes 1-8)
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(137));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("14kbps")));

    ApplicationContainer clientApps;
    for (int i = 1; i < 9; ++i) {
        clientApps.Add(onOffHelper.Install(nodes.Get(i)));
        clientApps.Get(i - 1)->Start(Seconds(2.0));
        clientApps.Get(i - 1)->Stop(Seconds(10.0));
    }

    // Enable global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("star");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}