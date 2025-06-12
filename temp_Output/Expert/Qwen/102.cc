#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint16_t port = 9;
    double simulationTime = 10.0;
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 1024;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable log components", verbose);
    cmd.AddValue("dataRate", "Data rate for OnOff applications", dataRate);
    cmd.AddValue("packetSize", "Packet size for OnOff applications", packetSize);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5); // A, B, C, D, E

    // Connect all nodes with point-to-point links except blacklisted ones
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1)); // A-B allowed
    NetDeviceContainer devicesAC = p2p.Install(nodes.Get(0), nodes.Get(2)); // A-C allowed
    NetDeviceContainer devicesBD = p2p.Install(nodes.Get(1), nodes.Get(3)); // B-D allowed
    NetDeviceContainer devicesCE = p2p.Install(nodes.Get(2), nodes.Get(4)); // C-E allowed
    // Blacklist: A-D and A-E are not connected directly

    // Install Internet stack
    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;

    Ipv4StaticRoutingHelper ipv4StaticRouting;
    listRH.Add(ipv4StaticRouting, 0);

    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesAC = address.Assign(devicesAC);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesBD = address.Assign(devicesBD);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesCE = address.Assign(devicesCE);

    // Set up multicast group address
    Ipv4Address multicastGroup("226.0.0.1");

    // Configure static multicast routes on node A
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingA = ipv4StaticRouting.GetStaticRouting(ipv4A);
    routingA->AddMulticastRoute(Ipv4Address::GetAny(), multicastGroup, interfacesAB.GetAddress(0), 1);
    routingA->AddMulticastRoute(Ipv4Address::GetAny(), multicastGroup, interfacesAC.GetAddress(0), 2);

    // Enable multicast reception at B, C, D, E
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4Node = nodes.Get(i)->GetObject<Ipv4>();
        ipv4Node->GetInterface(0)->SetReceiveCallback(MakeCallback(&Ipv4::LocalDeliver, ipv4Node));
    }

    // Setup packet sink applications
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(multicastGroup, port));
    ApplicationContainer sinks;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        sinkHelper.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
        ApplicationContainer app = sinkHelper.Install(nodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinks.Add(app);
    }

    // Setup OnOff application on node A to send multicast packets
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(multicastGroup, port));
    onoff.SetConstantRate(DataRate(dataRate), packetSize);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    p2p.EnablePcapAll("multicast_simulation");

    // Run simulation
    Simulator::Run();

    // Output packet statistics
    uint64_t totalRxBytes = 0;
    uint32_t totalPackets = 0;
    for (uint32_t i = 0; i < sinks.GetN(); ++i) {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinks.Get(i));
        uint64_t rxBytes = sink->GetTotalRx();
        uint32_t packets = rxBytes / packetSize;
        totalRxBytes += rxBytes;
        totalPackets += packets;
        NS_LOG_INFO("Node " << i+1 << " received " << rxBytes << " bytes (" << packets << " packets)");
    }
    NS_LOG_INFO("Total packets received: " << totalPackets);

    Simulator::Destroy();
    return 0;
}