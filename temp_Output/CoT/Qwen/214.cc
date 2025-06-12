#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 1 AP and 3 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    // Create Wi-Fi channel and MAC layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    // Setup AP
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Setup STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility models for all nodes (static)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Set up packet sink on STA 1 (node 1)
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up OnOff application from STA 0 to STA 1
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app = onoff.Install(wifiStaNodes.Get(0));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcap("wireless_network_simulation_ap", apDevice.Get(0));
    phy.EnablePcap("wireless_network_simulation_sta0", staDevices.Get(0));
    phy.EnablePcap("wireless_network_simulation_sta1", staDevices.Get(1));
    phy.EnablePcap("wireless_network_simulation_sta2", staDevices.Get(2));

    // Setup FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print performance metrics
    flowMonitor->CheckForLostPackets();
    std::cout << "\nPerformance Metrics:\n";
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowId id = it->first;
        FlowMonitor::FlowStats s = it->second;
        std::cout << "Flow ID: " << id << "\n";
        std::cout << "  Tx Packets: " << s.txPackets << "\n";
        std::cout << "  Rx Packets: " << s.rxPackets << "\n";
        std::cout << "  Lost Packets: " << s.lostPackets << "\n";
        std::cout << "  Throughput: " << s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
        std::cout << "  Packet Loss Ratio: " << static_cast<double>(s.lostPackets) / (s.txPackets + s.lostPackets) << "\n\n";
    }

    Simulator::Destroy();
    return 0;
}