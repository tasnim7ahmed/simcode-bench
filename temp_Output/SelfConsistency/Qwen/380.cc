#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiUdpSimulation");

int main(int argc, char *argv[])
{
    // Disable RTS/CTS for 802.11n
    Config::SetDefault("ns3::WifiMac::RTSThreshold", UintegerValue(999999));

    // Create nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Set up PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(WifiChannelHelper::DefaultChannel());

    // Set up MAC
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac");
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP server on AP node (Node 2)
    UdpServerHelper server(9); // port 9
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on STA node (Node 1)
    UdpClientHelper client(apInterface.GetAddress(0), 9); // send to port 9
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // unlimited packets
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("wifi-udp-simulation", apDevice.Get(0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}