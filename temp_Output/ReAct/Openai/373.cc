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
    Time::SetResolution(Time::NS);

    // Create 3 nodes: n0=AP, n1=STA1(Sender), n2=STA2(Receiver)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // n1, n2
    NodeContainer wifiApNode;
    wifiApNode.Create(1);   // n0

    // Wifi PHY and Channel setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);

    WifiMacHelper mac;

    Ssid ssid = Ssid ("ns3-wifi");

    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (10.0),
                                  "DeltaY", DoubleValue (10.0),
                                  "GridWidth", UintegerValue (3),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // UDP server on Node 2 (n2)
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (wifiStaNodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // UDP client on Node 1 (n1) sends to Node 2
    UdpClientHelper client (staInterfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (1000));
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}