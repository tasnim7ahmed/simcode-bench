#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpSimpleExample");

int main (int argc, char *argv[])
{
    // Enable console logging for debugging (can be commented out)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: STA and AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create (1);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    // Configure Wi-Fi MAC and standard
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");

    // Set up STA
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiStaNode);

    // Set up AP
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // Mobility for STA: Random within 100x100m²
    MobilityHelper mobility;

    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue ("Time"),
                              "Time", TimeValue (Seconds (1.0)),
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
    mobility.Install (wifiStaNode);

    // AP is stationary
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install (wifiApNode);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install (wifiStaNode);
    stack.Install (wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign (staDevice);
    apInterface = address.Assign (apDevice);

    // Set up UDP server (PacketSink) on AP, port 9
    uint16_t port = 9;
    Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", apLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install (wifiApNode.Get (0));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (20.0));

    // Set up UDP client on STA
    UdpClientHelper client (apInterface.GetAddress(0), port);
    client.SetAttribute ("MaxPackets", UintegerValue (10));
    client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApp = client.Install (wifiStaNode.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (20.0));

    // Enable pcap tracing (optional)
    phy.EnablePcap ("wifi-udp-simple-ap", apDevice.Get (0));
    phy.EnablePcap ("wifi-udp-simple-sta", staDevice.Get (0));

    Simulator::Stop (Seconds (20.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}