#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    // Log component settings
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create hub node
    Ptr<Node> hub = CreateObject<Node>();

    // Create spoke nodes
    NodeContainer spokes;
    spokes.Create(8);

    // Combine all nodes for internet stack installation
    NodeContainer allNodes(hub, spokes);

    // Setup point-to-point links between hub and each spoke
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        NetDeviceContainer link = p2p.Install(hub, spokes.Get(i));
        hubDevices.Add(link.Get(0));
        spokeDevices.Add(link.Get(1));
    }

    // Install Internet Stack
    InternetStackHelper internet;
    allNodes.Install(internet);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i) {
        Ipv4InterfaceContainer interfaces = ipv4.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
        hubInterfaces.Add(interfaces.Get(0));
        spokeInterfaces.Add(interfaces.Get(1));
        ipv4.NewNetwork();
    }

    // Enable global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up packet sink on the hub node
    uint16_t sinkPort = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(hub);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Configure OnOff applications on spoke nodes to send traffic to the hub
    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(hubInterfaces.GetAddress(0), sinkPort));
    onoff.SetConstantRate(DataRate("14kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(137));

    ApplicationContainer spokeApps;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        ApplicationContainer app = onoff.Install(spokes.Get(i));
        app.Start(Seconds(1.0));  // Start after sink is ready
        app.Stop(Seconds(10.0));
        spokeApps.Add(app);
    }

    // Enable pcap tracing on all devices
    p2p.EnablePcapAll("star_topology");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}