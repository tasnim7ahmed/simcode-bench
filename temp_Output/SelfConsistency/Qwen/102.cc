#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-multicast-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    Time stopTime = Seconds(10.0);
    uint32_t packetSize = 1024;
    DataRate dataRate("1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);  // A (0), B (1), C (2), D (3), E (4)

    // Create point-to-point links between specific nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devA_B = p2p.Install(nodes.Get(0), nodes.Get(1)); // A <-> B
    NetDeviceContainer devB_C = p2p.Install(nodes.Get(1), nodes.Get(2)); // B <-> C
    NetDeviceContainer devC_D = p2p.Install(nodes.Get(2), nodes.Get(3)); // C <-> D
    NetDeviceContainer devC_E = p2p.Install(nodes.Get(2), nodes.Get(4)); // C <-> E

    InternetStackHelper internet;
    internet.SetRoutingAlgorithm("ns3::Ipv4ListRoutingHelper");
    internet.InstallAll();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer ifA_B = ipv4.Assign(devA_B);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifB_C = ipv4.Assign(devB_C);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifC_D = ipv4.Assign(devC_D);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifC_E = ipv4.Assign(devC_E);

    // Set up multicast group address
    Ipv4Address multicastGroup("226.0.0.1");

    // Enable multicast routing
    Ipv4StaticRoutingHelper multicastRouting;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4Node = nodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> routingProto = multicastRouting.Create(ipv4Node);
        ipv4Node->SetRoutingProtocol(routingProto);
    }

    // Configure multicast routes manually
    // Node B forwards to C
    // Node C forwards to D and E
    // Nodes D and E do not forward further
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {  // skip node A
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4StaticRouting> routing = DynamicCast<Ipv4StaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        if (!routing) continue;

        if (node == nodes.Get(1)) {  // Node B
            routing->AddMulticastRoute(multicastGroup, ifB_C.GetAddress(1, 0), 2);
        } else if (node == nodes.Get(2)) {  // Node C
            routing->AddMulticastRoute(multicastGroup, ifC_D.GetAddress(0, 0), 2);
            routing->AddMulticastRoute(multicastGroup, ifC_E.GetAddress(1, 0), 3);
        } else if (node == nodes.Get(3)) {  // Node D: receive only
            routing->AddMulticastLocalRoute(multicastGroup, ifC_D.GetAddress(1, 0));
        } else if (node == nodes.Get(4)) {  // Node E: receive only
            routing->AddMulticastLocalRoute(multicastGroup, ifC_E.GetAddress(0, 0));
        }
    }

    // Install packet sink applications on B, C, D, E
    uint16_t port = 9;
    ApplicationContainer sinks;

    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer app = sinkHelper.Install(nodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(stopTime);
        sinks.Add(app);
    }

    // Setup OnOff application on node A to send multicast
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    onoff.SetConstantRate(DataRate("500kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(stopTime);

    // Pcap tracing
    if (tracing) {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("multicast-simulation.tr"));
        p2p.EnablePcapAll("multicast-simulation");
    }

    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();

    // Output statistics
    std::cout << "\nReceiver Statistics:\n";
    for (uint32_t i = 0; i < sinks.GetN(); ++i) {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinks.Get(i));
        std::cout << "Node " << i+1 << ": Received " << sink->GetTotalRx() << " bytes\n";
    }

    return 0;
}