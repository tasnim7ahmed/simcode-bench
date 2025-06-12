#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create six sensor nodes
    NodeContainer sensorNodes;
    sensorNodes.Create(6);

    // Set up the physical layer with LR-WPAN (IEEE 802.15.4)
    LrWpanHelper lrWpan;
    NetDeviceContainer sensorDevices = lrWpan.Install(sensorNodes);
    lrWpan.AssociateToPan(sensorDevices, 0);

    // Set up the 6LoWPAN layer (optional)
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(sensorDevices);

    // Install the internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(sensorNodes);

    // Assign IP addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sensorInterfaces = ipv6.Assign(sixlowpanDevices);

    // Set up mobility: Arrange nodes in a circle
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Sink node (center)
    double radius = 20.0;
    for (int i = 1; i < 6; ++i)
    {
        double angle = i * (2 * M_PI / 5); // 5 peripheral nodes
        positionAlloc->Add(Vector(radius * cos(angle), radius * sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    // Set up UDP echo server on the central (sink) node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sensorNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP echo clients on the peripheral sensor nodes
    UdpEchoClientHelper echoClient(sensorInterfaces.GetAddress(0, 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (int i = 1; i < 6; ++i)
    {
        clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable tracing
    lrWpan.EnablePcapAll("wsn-example");

    // Set up FlowMonitor to gather performance metrics
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Set up NetAnim for visualization
    AnimationInterface anim("wsn-animation.xml");

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // FlowMonitor statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / 20.0 / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  End-to-End Delay: " << it->second.delaySum.GetSeconds() << " seconds" << std::endl;
    }

    // Clean up
    Simulator::Destroy();
    return 0;
}

