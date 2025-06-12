#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: one AP and two STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Create channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up MAC and PHY for AP and STA
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    Ssid ssid = Ssid("ns-3-ssid");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // STA configuration
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

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Setup UDP traffic from stations to AP
    uint16_t port = 9; // Discard port (sink)

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = client.Install(wifiStaNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = client.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcap("wifi_ap", apDevices.Get(0));
    phy.EnablePcap("wifi_sta1", staDevices.Get(0));
    phy.EnablePcap("wifi_sta2", staDevices.Get(1));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();

    // Print throughput stats
    uint64_t totalBytesSta1 = DynamicCast<UdpServer>(serverApps.Get(0))->GetReceived());
    uint64_t totalBytesSta2 = DynamicCast<UdpServer>(serverApps.Get(0))->GetReceived());

    std::cout << "STA 1: Received " << totalBytesSta1 << " bytes" << std::endl;
    std::cout << "Throughput STA 1: " << (totalBytesSta1 * 8.0) / (10.0 - 2.0) / 1e6 << " Mbps" << std::endl;

    std::cout << "STA 2: Received " << totalBytesSta2 << " bytes" << std::endl;
    std::cout << "Throughput STA 2: " << (totalBytesSta2 * 8.0) / (10.0 - 3.0) / 1e6 << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}