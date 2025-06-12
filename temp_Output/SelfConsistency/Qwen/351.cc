#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpServerClientSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for UdpServer and UdpClient
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);

    // Create nodes: Node 0 as AP, Node 1 as client
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Setup Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and Wi-Fi
    WifiMacHelper mac;
    WifiHelper wifi;

    // Set default algorithm to ConstantRate
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    // Install AP
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install STA
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Mobility model - constant position
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0", "0.0.0.1");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Setup UDP server on AP at port 8080
    UdpServerHelper server(8080);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP client on STA sending to AP's IP
    UdpClientHelper client(apInterface.GetAddress(0), 8080);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("udp-wifi-ap", apDevice.Get(0));
    phy.EnablePcap("udp-wifi-sta", staDevice.Get(0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}