#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("WifiTcpExample", LOG_LEVEL_INFO);

    // Create nodes: 1 AP + 2 Stations (STAs)
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(2);
    wifiApNode.Create(1);

    // Configure Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcs9"),
                                 "ControlMode", StringValue("VhtMcs0"));

    // Configure MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211ac");

    // Configure STA devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(wifiStaNodes);
    internet.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

    // Setup TCP server on STA 1
    uint16_t port = 5000;
    PacketSinkHelper tcpServer("ns3::TcpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = tcpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup TCP client on STA 2
    OnOffHelper tcpClient("ns3::TcpSocketFactory",
                          InetSocketAddress(staInterfaces.GetAddress(0), port));
    tcpClient.SetAttribute("DataRate", StringValue("100Mbps"));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = tcpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcapAll("wifi-tcp");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
