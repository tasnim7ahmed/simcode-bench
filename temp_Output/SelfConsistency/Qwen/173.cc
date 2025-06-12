#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t numWiredNodes = 4;
    uint32_t numWirelessNodes = 3;
    double simulationTime = 10.0;

    // Create wired nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(numWiredNodes);

    // Create wireless nodes
    NodeContainer wirelessNodes;
    wirelessNodes.Create(numWirelessNodes);

    // Create point-to-point links between wired nodes (chain topology)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices;
    for (uint32_t i = 0; i < numWiredNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        wiredDevices.Add(p2p.Install(pair));
    }

    // Setup Wi-Fi for wireless nodes
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")));
    wifiPhy.Set("ChannelWidth", UintegerValue(20));

    NetDeviceContainer wirelessDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    // Create AP node (use one of the wired nodes as gateway)
    NodeContainer apNode = wiredNodes.Get(0); // Gateway node
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")),
                    "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    // Mobility model for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wirelessNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer wiredInterfaces;
    for (uint32_t i = 0; i < wiredDevices.GetN(); ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(wiredDevices.Get(i));
        wiredInterfaces.Add(ifc);
        address.NewNetwork();
    }

    Ipv4InterfaceContainer wirelessInterfaces = address.Assign(wirelessDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set up default routes via the gateway
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP traffic from wireless to wired nodes
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(wiredInterfaces.GetAddress(1, 0), sinkPort)); // First wired link's second node

    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSink.Install(wiredNodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numWirelessNodes; ++i) {
        clientApps.Add(onoff.Install(wirelessNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    wifiPhy.EnablePcap("wireless_node", wirelessDevices);
    p2p.EnablePcapAll("wired_link");

    // Setup Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate and print throughput
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStats stats = monitor->GetFlowStats().begin()->second;

    double throughput = (stats.rxBytes * 8) / (simulationTime * 1000000.0); // Mbps
    std::cout << "TCP Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}