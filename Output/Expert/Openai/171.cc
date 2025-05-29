#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvPdrE2eDelay");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(
                                CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"))));
    mobility.Install(nodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 4000;
    double appStartMin = 1.0;
    double appStartMax = 5.0;
    Ptr<UniformRandomVariable> randVar = CreateObject<UniformRandomVariable>();

    std::vector<ApplicationContainer> clientApps, serverApps;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        // Select random destination (not self)
        uint32_t dst;
        do {
            dst = randVar->GetInteger(0, numNodes-1);
        } while (dst == i);

        UdpServerHelper server(port + i);
        ApplicationContainer serverApp = server.Install(nodes.Get(dst));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));
        serverApps.push_back(serverApp);

        UdpClientHelper client(interfaces.GetAddress(dst), port + i);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        double startTime = randVar->GetValue(appStartMin, appStartMax);
        clientApp.Start(Seconds(startTime));
        clientApp.Stop(Seconds(simTime));
        clientApps.push_back(clientApp);
    }

    phy.EnablePcapAll("adhoc-aodv");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint32_t rxPackets = 0;
    uint32_t txPackets = 0;
    double delaySum = 0.0;
    uint32_t deliveredFlows = 0;

    for (auto const& flow : stats)
    {
        txPackets += flow.second.txPackets;
        rxPackets += flow.second.rxPackets;
        if (flow.second.rxPackets > 0)
        {
            delaySum += (flow.second.delaySum.GetSeconds());
            deliveredFlows++;
        }
    }

    double pdr = txPackets > 0 ? ((double)rxPackets / txPackets) * 100.0 : 0.0;
    double avgDelay = rxPackets > 0 ? delaySum / rxPackets : 0.0;

    std::cout << "Packet Delivery Ratio (PDR): " << pdr << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}