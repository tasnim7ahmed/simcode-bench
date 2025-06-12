#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/energy-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numSensors = 5;
    double radius = 30.0;
    double simTime = 20.0;

    NodeContainer nodes;
    nodes.Create(numSensors + 1); // 5 sensors + 1 sink

    // Mobility: sink at center, sensors in a circle
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // sink at center
    for (uint32_t i = 0; i < numSensors; ++i)
    {
        double angle = 2 * M_PI * i / numSensors;
        double x = 50.0 + radius * std::cos(angle);
        double y = 50.0 + radius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 802.15.4 (LR-WPAN)
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

    // Energy source (optional; not essential but typical for sensors)
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    EnergySourceContainer energySources = energySourceHelper.Install(nodes);

    LrWpanRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(lrwpanDevices, energySources);

    // Install Internet stack and assign addresses
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ipv6Ifs = ipv6.Assign(lrwpanDevices);
    for (uint32_t i = 0; i < ipv6Ifs.GetN(); ++i)
    {
        ipv6Ifs.SetForwarding(i, true);
        ipv6Ifs.SetDefaultRouteInAllNodes(i);
    }

    // UDP Server on sink node (first node)
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Clients on sensors (nodes 1-5)
    double interPacketInterval = 2.0;
    uint32_t maxPacketCount = static_cast<uint32_t>(simTime / interPacketInterval);

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i <= numSensors; ++i)
    {
        UdpClientHelper client(ipv6Ifs.GetAddress(0, 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("PacketSize", UintegerValue(40));
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable global routing for IPv6
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing
    lrWpanHelper.EnablePcapAll("sensor-network");

    // NetAnim setup
    AnimationInterface anim("sensor-network.xml");
    anim.SetMobilityPollInterval(Seconds(1));
    anim.UpdateNodeDescription(nodes.Get(0), "Sink");
    anim.UpdateNodeColor(nodes.Get(0), 255, 0, 0);
    for (uint32_t i = 1; i <= numSensors; ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "Sensor");
        anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);
    }

    // FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));

    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    uint32_t totalRx = 0;
    uint32_t totalTx = 0;
    double throughput = 0.0;
    double avgDelay = 0.0;
    uint32_t receivedFlows = 0;

    for (auto const &flow : stats)
    {
        Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.destinationAddress == ipv6Ifs.GetAddress(0, 1)) && (t.destinationPort == port))
        {
            totalTx += flow.second.txPackets;
            totalRx += flow.second.rxPackets;
            avgDelay += (flow.second.rxPackets > 0) ? flow.second.delaySum.GetSeconds() / flow.second.rxPackets : 0;
            throughput += (flow.second.rxBytes * 8.0) / (simTime - 1);
            receivedFlows++;
        }
    }

    if (receivedFlows > 0)
    {
        avgDelay = avgDelay / receivedFlows;
        throughput = throughput / receivedFlows;
    }

    std::cout << "Total Packets Sent: " << totalTx << std::endl;
    std::cout << "Total Packets Received: " << totalRx << std::endl;
    std::cout << "Packet Delivery Ratio: "
              << ((totalTx > 0) ? (double(totalRx) / totalTx) * 100 : 0) << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput: " << throughput / 1000 << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}