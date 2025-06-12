#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiApStaUdpExample");

int main (int argc, char *argv[])
{
    // Enable logging
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 10.0;

    // Create nodes: 1 AP + 3 STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (3);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    // Configure Wi-Fi physical and MAC layers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("my-wifi-ssid");

    NetDeviceContainer staDevices, apDevice;

    // Configure STA devices
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // Mobility for STAs: RandomWalk in a 50x50 box
    MobilityHelper mobility;

    mobility.SetPositionAllocator (
        "ns3::RandomRectanglePositionAllocator",
        "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
        "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]")
    );
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)),
                              "Distance", DoubleValue (10));
    mobility.Install (wifiStaNodes);

    // Mobility for AP: static in middle
    MobilityHelper apMobility;
    apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    apMobility.Install (wifiApNode);
    Ptr<MobilityModel> apMobilityModel = wifiApNode.Get (0)->GetObject<MobilityModel> ();
    apMobilityModel->SetPosition (Vector (25.0, 25.0, 0.0));

    // Install Internet stack and assign addresses
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

    // Install UDP server on AP
    uint16_t port = 9;
    UdpServerHelper udpServer (port);
    ApplicationContainer serverApp = udpServer.Install (wifiApNode.Get (0));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (simTime));

    // Install UDP clients on each STA
    for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
        UdpClientHelper udpClient (apInterface.GetAddress (0), port);
        udpClient.SetAttribute ("MaxPackets", UintegerValue (5));
        udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (i));
        clientApp.Start (Seconds (1.0));
        clientApp.Stop (Seconds (simTime));
    }

    // Enable pcap tracing (optional)
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap ("wifi-ap", apDevice.Get (0), true);
    phy.EnablePcap ("wifi-sta1", staDevices.Get (0), true);
    phy.EnablePcap ("wifi-sta2", staDevices.Get (1), true);
    phy.EnablePcap ("wifi-sta3", staDevices.Get (2), true);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}