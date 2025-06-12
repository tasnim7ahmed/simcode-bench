#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging (optional)
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create the hub node and spoke nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer spokes;
    spokes.Create(8);

    // Combine all nodes into one container for internet stack installation
    NodeContainer allNodes = NodeContainer(hub, spokes);

    // Point-to-point helper to create links between hub and each spoke
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        NodeContainer pair(hub.Get(0), spokes.Get(i));
        NetDeviceContainer devices = p2p.Install(pair);
        hubDevices.Add(devices.Get(0));
        spokeDevices.Add(devices.Get(1));
    }

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        address.NewNetwork();
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
        hubInterfaces.Add(interfaces.Get(0));
        spokeInterfaces.Add(interfaces.Get(1));
    }

    // Set up Packet Sink (TCP receiver) on hub node
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(hubInterfaces.GetAddress(0), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(hub.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Configure OnOff applications on each spoke node
    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("14kbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(137));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer app = onoff.Install(spokes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));
    }

    // Enable static global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing for all devices
    for (uint32_t i = 0; i < hubDevices.GetN(); ++i) {
        std::string fileName = "hub_spoke_" + std::to_string(i);
        pointToPoint.EnablePcapAll(fileName);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}