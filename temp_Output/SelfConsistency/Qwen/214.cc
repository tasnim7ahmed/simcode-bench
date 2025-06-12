#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessFourNodeSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable packet tracing and logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes: one AP, three STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    // Create Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up MAC and PHY for Wi-Fi
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);

    // Configure the AP
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure the STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
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

    // Set up applications
    uint16_t port = 9; // Echo port

    // Install a UDP echo server on STA 0
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo clients on STA 1 and STA 2, sending to STA 0
    Time interPacketInterval = Seconds(0.1);
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 20;

    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp1 = echoClient.Install(wifiStaNodes.Get(1));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    ApplicationContainer clientApp2 = echoClient.Install(wifiStaNodes.Get(2));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("wireless_four_node_ap", apDevice.Get(0));
    phy.EnablePcap("wireless_four_node_sta0", staDevices.Get(0));
    phy.EnablePcap("wireless_four_node_sta1", staDevices.Get(1));
    phy.EnablePcap("wireless_four_node_sta2", staDevices.Get(2));

    // Setup FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print per-flow statistics
    flowMonitor->CheckForLostPackets();
    std::cout << "\n\nFlow Monitor Results:\n";
    flowMonitor->SerializeToXmlFile("flowmonitor-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}