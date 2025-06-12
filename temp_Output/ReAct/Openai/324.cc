#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHttpSimulation");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer wifiNodes;
    wifiNodes.Create (2);

    // Configure Wi-Fi (802.11b)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("DsssRate11Mbps"),
                                 "ControlMode", StringValue ("DsssRate11Mbps"));
    WifiMacHelper mac;
    Ssid ssid = Ssid ("simple-wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (5.0, 5.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    NetDeviceContainer devices;
    devices.Add (staDevice);
    devices.Add (apDevice);
    interfaces = address.Assign (devices);

    // HTTP simulation using TcpServer and TcpClient as HTTP is over TCP
    uint16_t httpPort = 80;
    // Install a packet sink ("HTTP server") on node 1
    Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), httpPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install (wifiNodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // OnOff application ("HTTP client") on node 0 to request a page
    OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (interfaces.GetAddress (1), httpPort));
    clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (512));
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (1024)); // Simulate single HTTP GET
    ApplicationContainer clientApp = clientHelper.Install (wifiNodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}