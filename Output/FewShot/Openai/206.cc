#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkStar");

int main(int argc, char *argv[]) {
    uint32_t numSensors = 5;
    double simTime = 20.0;
    double packetInterval = 2.0; // seconds

    // Create sink and sensor nodes
    NodeContainer sinkNode;
    sinkNode.Create(1);
    NodeContainer sensorNodes;
    sensorNodes.Create(numSensors);
    NodeContainer allNodes;
    allNodes.Add(sinkNode);
    allNodes.Add(sensorNodes);

    // Set up mobility: star topology
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Place sink at centre (50,50)
    positionAlloc->Add(Vector(50.0, 50.0, 0.0));
    double starRadius = 20.0;
    for (uint32_t i = 0; i < numSensors; ++i) {
        double angle = i * (2 * M_PI / numSensors);
        double x = 50.0 + starRadius * std::cos(angle);
        double y = 50.0 + starRadius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Install 802.15.4 devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(allNodes);
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

    // Set low-power mode (optional, for realistic setting)
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i) {
        Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(i));
        dev->GetPhy()->SetChannel(CreateObject<LrWpanChannel>());
    }

    // Install IPv6 stack (IPv4 is not supported by 6LoWPAN in ns-3)
    InternetStackHelper internetv6;
    internetv6.Install(allNodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrwpanDevices);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevices);

    // Make all links point-to-multipoint to the sink
    // Set up UDP Server at sink
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(sinkNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Set up UDP Clients at sensors
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSensors; ++i) {
        UdpClientHelper udpClient(interfaces.GetAddress(0,1), port); // sink address
        udpClient.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simTime/packetInterval)));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(40));
        ApplicationContainer app = udpClient.Install(sensorNodes.Get(i));
        app.Start(Seconds(1.0 + i * 0.1));
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    // Enable packet capture
    lrWpanHelper.EnablePcapAll("sensor-network-star");

    // Set up FlowMonitor to measure packet delivery, latency, throughput
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // NetAnim configuration
    AnimationInterface anim("sensor-network-star.xml");
    anim.SetConstantPosition(sinkNode.Get(0), 50.0, 50.0);
    for (uint32_t i = 0; i < numSensors; ++i) {
        Ptr<Node> node = sensorNodes.Get(i);
        anim.SetConstantPosition(node, 
            positionAlloc->GetNext()->x, positionAlloc->GetNext()->y);
    }

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    // Print statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flowmonHelper.GetClassifier6());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.destinationPort == port) {
            std::cout << "Flow: " << it->first 
                << " SrcAddr=" << t.sourceAddress 
                << " DstAddr=" << t.destinationAddress << std::endl;
            std::cout << "  Tx Packets=" << it->second.txPackets << std::endl;
            std::cout << "  Rx Packets=" << it->second.rxPackets << std::endl;
            std::cout << "  Lost Packets=" << it->second.lostPackets << std::endl;
            std::cout << "  Throughput: " 
                      << (it->second.rxBytes * 8.0 / (simTime * 1000.0)) << " kbps" << std::endl;
            if (it->second.rxPackets > 0)
                std::cout << "  Mean Delay: "
                          << (it->second.delaySum.GetSeconds() / it->second.rxPackets) << " s" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}