#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    uint32_t numNodes = 4;
    double simTime = 30.0;
    double areaX = 100.0;
    double areaY = 100.0;
    double nodeSpeed = 2.0;
    double nodePause = 1.0;
    double udpAppStart = 2.0;
    double udpAppStop = simTime - 1.0;
    uint16_t udpPort = 50000;

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
        "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.Install(nodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint32_t packetSize = 512;
    uint32_t packetsPerSec = 10;

    ApplicationContainer udpApps;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = 0; j < numNodes; ++j)
        {
            if (i != j)
            {
                UdpServerHelper server(udpPort + i * numNodes + j);
                ApplicationContainer servApp = server.Install(nodes.Get(j));
                servApp.Start(Seconds(udpAppStart));
                servApp.Stop(Seconds(udpAppStop));
                uint16_t dstPort = udpPort + i * numNodes + j;
                UdpClientHelper client(interfaces.GetAddress(j), dstPort);
                client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
                client.SetAttribute("Interval", TimeValue(Seconds(1.0 / packetsPerSec)));
                client.SetAttribute("PacketSize", UintegerValue(packetSize));
                ApplicationContainer clientApp = client.Install(nodes.Get(i));
                clientApp.Start(Seconds(udpAppStart + 0.1 * i));
                clientApp.Stop(Seconds(udpAppStop));
                udpApps.Add(clientApp);
            }
        }
    }

    AnimationInterface anim("aodv-adhoc-netanim.xml");
    anim.SetMobilityPollInterval(Seconds(1.0));
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "Node" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();

    uint64_t totalRxPackets = 0;
    uint64_t totalTxPackets = 0;
    uint64_t totalRxBytes = 0;
    double totalDelay = 0.0;
    uint64_t totalDeliveredFlows = 0;

    for (auto &flow : stats)
    {
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalRxBytes += flow.second.rxBytes;
        if (flow.second.rxPackets > 0)
        {
            totalDelay += flow.second.delaySum.GetSeconds();
            totalDeliveredFlows += flow.second.rxPackets;
        }
    }

    double pdr = totalTxPackets > 0 ? (double)totalRxPackets / (double)totalTxPackets : 0.0;
    double throughput = (simTime > 0) ? (double)totalRxBytes * 8.0 / simTime / 1000.0 : 0.0;
    double avgDelay = totalDeliveredFlows > 0 ? totalDelay / totalDeliveredFlows : 0.0;

    std::cout << "Packet Delivery Ratio: " << pdr * 100 << " %" << std::endl;
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay * 1000 << " ms" << std::endl;

    Simulator::Destroy();
    return 0;
}