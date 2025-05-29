#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <vector>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvPdrSimulation");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("AdhocAodvPdrSimulation", LOG_LEVEL_INFO);

    uint32_t nNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Install wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(
                                 CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]")
                                 )));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(50.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP traffic
    uint16_t port = 4000;
    uint32_t nCbrPairs = nNodes / 2;
    double appStartMin = 1.0;
    double appStartMax = 5.0;

    // To avoid duplicates, pick unique src/dst pairs
    std::vector<uint32_t> nodeIndexes(nNodes);
    std::iota(nodeIndexes.begin(), nodeIndexes.end(), 0);
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();

    for (uint32_t i = 0; i < nCbrPairs; ++i)
    {
        uint32_t srcIdx, dstIdx;
        do
        {
            srcIdx = uv->GetInteger(0, nNodes - 1);
            dstIdx = uv->GetInteger(0, nNodes - 1);
        } while (srcIdx == dstIdx);

        // UDP Server on dst
        UdpServerHelper server(port + i);
        ApplicationContainer serverApp = server.Install(nodes.Get(dstIdx));
        serverApp.Start(Seconds(0.5));
        serverApp.Stop(Seconds(simTime));

        // UDP Client on src
        UdpClientHelper client(interfaces.GetAddress(dstIdx), port + i);
        client.SetAttribute("MaxPackets", UintegerValue(32000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = client.Install(nodes.Get(srcIdx));
        double appStartTime = uv->GetValue(appStartMin, appStartMax);
        clientApp.Start(Seconds(appStartTime));
        clientApp.Stop(Seconds(simTime - 0.1));
    }

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("adhoc-aodv");

    // Set up FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Calculate and output PDR and Avg E2E Delay
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    double totalDelay = 0.0;

    for (auto const &flow : stats)
    {
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
    }

    double pdr = (totalTxPackets > 0) ? 100.0 * totalRxPackets / totalTxPackets : 0.0;
    double avgDelay = (totalRxPackets > 0) ? totalDelay / totalRxPackets : 0.0;

    std::cout << "Simulation Results" << std::endl;
    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}