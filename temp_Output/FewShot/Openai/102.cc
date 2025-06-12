#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-multicast-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastStaticRoutingExample");

void PrintPacketStats(Ptr<Application> sinkApp) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp);
    std::cout << "Total received packets: " << sink->GetTotalRx() << " bytes." << std::endl;
}

int main(int argc, char *argv[]) {
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create five nodes: A, B, C, D, E
    NodeContainer nodes;
    nodes.Create(5); // 0:A, 1:B, 2:C, 3:D, 4:E

    // Create Csma channels (star topology with A as root)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));

    NodeContainer nA_B, nA_C, nA_D, nA_E;
    nA_B.Add(nodes.Get(0)); nA_B.Add(nodes.Get(1));
    nA_C.Add(nodes.Get(0)); nA_C.Add(nodes.Get(2));
    nA_D.Add(nodes.Get(0)); nA_D.Add(nodes.Get(3));
    nA_E.Add(nodes.Get(0)); nA_E.Add(nodes.Get(4));

    NetDeviceContainer dA_B = csma.Install(nA_B);
    NetDeviceContainer dA_C = csma.Install(nA_C);
    NetDeviceContainer dA_D = csma.Install(nA_D);
    NetDeviceContainer dA_E = csma.Install(nA_E);

    // Install internet stacks with multicast and static routing
    InternetStackHelper internet;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper listRH;
    Ipv4MulticastRoutingHelper multicastRH;
    listRH.Add(staticRouting, 0);
    listRH.Add(multicastRH, 10);
    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    // Assign IP addresses for each "spoke"
    Ipv4AddressHelper ipAB, ipAC, ipAD, ipAE;
    ipAB.SetBase("10.1.1.0", "255.255.255.0");
    ipAC.SetBase("10.1.2.0", "255.255.255.0");
    ipAD.SetBase("10.1.3.0", "255.255.255.0");
    ipAE.SetBase("10.1.4.0", "255.255.255.0");

    Ipv4InterfaceContainer iAB = ipAB.Assign(dA_B);
    Ipv4InterfaceContainer iAC = ipAC.Assign(dA_C);
    Ipv4InterfaceContainer iAD = ipAD.Assign(dA_D);
    Ipv4InterfaceContainer iAE = ipAE.Assign(dA_E);

    // Set up the multicast group address
    Ipv4Address multicastGroup("225.1.2.4");
    uint16_t multicastPort = 5000;

    // Configure static routing and blacklist specific links
    // Let's blacklist A-C and A-E direct communication (for demonstration)
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRoutingB = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRoutingC = staticRouting.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRoutingD = staticRouting.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticRoutingE = staticRouting.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());

    // Multicast routes: join on B, C, D, E
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4D = nodes.Get(3)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4E = nodes.Get(4)->GetObject<Ipv4>();

    // Install group membership
    ipv4B->JoinMulticastGroup(1, multicastGroup); // 1 is the interface index on B
    ipv4C->JoinMulticastGroup(1, multicastGroup);
    ipv4D->JoinMulticastGroup(1, multicastGroup);
    ipv4E->JoinMulticastGroup(1, multicastGroup);

    // Node A's output interface: always 1
    staticRoutingA->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.1.2"), 1); // to B
    staticRoutingA->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.3.2"), 1); // to D

    // Blacklist: To demonstrate, don't add routes to C and E from A directly

    // Accept on B, C, D, E
    staticRoutingB->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.1.2"), 1);
    staticRoutingC->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.2.2"), 1);
    staticRoutingD->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.3.2"), 1);
    staticRoutingE->AddMulticastRoute(1, multicastGroup, Ipv4Address("10.1.4.2"), 1);

    // Install PacketSink (UDP) on B, C, D, E to receive multicast packets
    ApplicationContainer sinks;
    for (uint32_t i = 1; i < 5; ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, multicastPort));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        sinks.Add(sinkApp);
    }

    // Sender: OnOffHelper on node A (send to multicast group)
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastGroup, multicastPort)));
    onoff.SetAttribute("DataRate", StringValue("400kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer senderApp = onoff.Install(nodes.Get(0));

    // Enable PCAP
    csma.EnablePcapAll("multicast-star");

    Simulator::Stop(Seconds(10.0));

    // At end, print stats for each sink
    Simulator::Schedule(Seconds(10.0), [&sinks]() {
        for (uint32_t i = 0; i < sinks.GetN(); ++i) {
            Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinks.Get(i));
            std::cout << "Node " << (i+1) << " received " << sink->GetTotalRx() << " bytes." << std::endl;
        }
    });

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}