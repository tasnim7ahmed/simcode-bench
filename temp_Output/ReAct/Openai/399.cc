#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    NodeContainer wifiNodes;
    wifiNodes.Create (2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"), "ControlMode", StringValue ("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

    MobilityHelper mobility;

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    InternetStackHelper stack;
    stack.Install (wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    NetDeviceContainer devices;
    devices.Add (staDevice);
    devices.Add (apDevice);

    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint16_t port = 9;

    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (wifiNodes.Get (1));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (10.0));

    uint32_t packetSize = 1024;
    double interval = 0.1; // seconds
    uint32_t maxPackets = 0; // unlimited

    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}