#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkTopology");

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simulationTime = 10.0; // seconds

    // Create wired nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4); // 4 wired nodes

    // Create wireless nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3); // 3 wireless nodes

    NodeContainer wifiApNode = wiredNodes.Get(3); // Use one wired node as AP

    // Create point-to-point connections between wired nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i)
    {
        NetDeviceContainer link = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices.Add(link);
    }

    // Set up Wi-Fi for wireless nodes
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    // Assign IP addresses to the wired network
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces = address.Assign(p2pDevices);

    // Assign IP addresses to the wireless network
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    // Set up mobility for wireless nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Enable global routing to route between wired and wireless sections
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create TCP traffic from wireless nodes to wired nodes
    uint16_t port = 9;
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        AddressValue remoteAddress(InetSocketAddress(wiredInterfaces.GetAddress(3), port));
        onOffHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(onOffHelper.Install(wifiStaNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wiredNodes.Get(3)); // Sink on last wired node
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    p2p.EnablePcapAll("hybrid-topology");
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta", staDevices.Get(0));

    // Flow monitor to calculate throughput
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output the throughput
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double totalThroughput = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        totalThroughput += i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000; // in Mbps
    }

    NS_LOG_UNCOND("Average Throughput: " << totalThroughput << " Mbps");

    // Clean up
    Simulator::Destroy();
    return 0;
}

