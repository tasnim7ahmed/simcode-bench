#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wifiNodes;
    wifiNodes.Create(3);

    // Create point-to-point links (wired)
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices1 = pointToPoint.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    NetDeviceContainer wiredDevices2 = pointToPoint.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    NetDeviceContainer wiredDevices3 = pointToPoint.Install(wiredNodes.Get(2), wiredNodes.Get(3));

    // Create Wi-Fi links (wireless)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, wiredNodes.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(wiredNodes);
    internet.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces1 = address.Assign(wiredDevices1);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces2 = address.Assign(wiredDevices2);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces3 = address.Assign(wiredDevices3);
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    // Enable IPv4 global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create TCP application (client on wifi, server on wired)
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wiredNodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(Ipv4Address("10.1.3.2"), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Time("1ms")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (int i = 0; i < 3; ++i) {
        clientApps.Add(client.Install(wifiNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Create pcap tracing
    pointToPoint.EnablePcapAll("hybrid_network");
    wifiPhy.EnablePcapAll("hybrid_network");

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalBytes = 0.0;
    for (auto const& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == Ipv4Address("10.1.3.2")) {
             totalBytes += flow.second.bytesTx;
        }
    }

    double throughput = (totalBytes * 8.0) / (10.0 - 2.0) / 1000000; // Mbit/s

    std::cout << "Average throughput: " << throughput << " Mbit/s" << std::endl;

    Simulator::Destroy();
    return 0;
}