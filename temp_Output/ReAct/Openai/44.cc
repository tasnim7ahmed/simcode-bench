#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInfraFixedRssSimulation");

int main(int argc, char *argv[])
{
    // User-configurable parameters
    double rss = -60.0; // dBm
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue ("rss", "Fixed RSS loss (dBm)", rss);
    cmd.AddValue ("packetSize", "Size of application packets (bytes)", packetSize);
    cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
    cmd.AddValue ("verbose", "Enable verbose logging", verbose);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable ("WifiInfraFixedRssSimulation", LOG_LEVEL_INFO);
    }

    // Create nodes: one STA and one AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi physical and MAC layers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-infra");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: static positions
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // UDP server on AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0 + numPackets * interval));

    // UDP client on STA, send to AP
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0 + numPackets * interval));

    // PCAP tracing
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi_infra_sta", staDevice.Get(0), true, true);
    phy.EnablePcap("wifi_infra_ap", apDevice.Get(0), true, true);

    // Simulation time
    Simulator::Stop(Seconds(12.0 + numPackets * interval));
    Simulator::Run();

    // Output received packet count
    Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
    std::cout << "Received " << udpServer->GetReceived() << " packets at AP (RSS=" << rss << " dBm)" << std::endl;

    Simulator::Destroy();
    return 0;
}