#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double simulationTime = 10.0;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Distance", DoubleValue(50));
    mobility.Install(nodes);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("aodv-adhoc.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    wifiPhy.EnablePcapAll("aodv-adhoc");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalDelay = 0, totalTx = 0, totalRx = 0, totalThroughput = 0;

    for (auto &[flowId, flowStats] : stats) {
        totalTx += flowStats.txPackets;
        totalRx += flowStats.rxPackets;
        totalDelay += flowStats.delaySum.ToDouble(Time::S);
        totalThroughput += (flowStats.rxBytes * 8.0) / (simulationTime * 1000 * 1000); // Mbps
    }

    double packetDeliveryRatio = (totalRx / totalTx) * 100.0;
    double averageEndToEndDelay = (totalDelay / totalRx);
    double avgThroughput = totalThroughput / stats.size();

    NS_LOG_UNCOND("=== Simulation Results ===");
    NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio << "%");
    NS_LOG_UNCOND("Average End-to-End Delay: " << averageEndToEndDelay << " s");
    NS_LOG_UNCOND("Average Throughput: " << avgThroughput << " Mbps");

    Simulator::Destroy();
    return 0;
}