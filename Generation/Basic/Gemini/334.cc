#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("DsssRate1Mbps"));
Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", StringValue ("7"));
Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSsrc", StringValue ("6"));

NodeContainer apNode;
apNode.Create (1);
NodeContainer staNode;
staNode.Create (1);

YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
YansWifiPhyHelper phy;
phy.SetChannel (channel.Create ());

WifiHelper wifi;
wifi.SetStandard (WIFI_STANDARD_80211n);

Ssid ssid = Ssid ("ns3-wifi-network");

WifiMacHelper apMac;
apMac.SetType ("ns3::ApWifiMac",
"Ssid", SsidValue (ssid),
"BeaconInterval", TimeValue (MicroSeconds (102400)));

NetDeviceContainer apDevice;
apDevice = wifi.Install (phy, apMac, apNode.Get (0));

WifiMacHelper staMac;
staMac.SetType ("ns3::StaWifiMac",
"Ssid", SsidValue (ssid),
"ListenBeforeTalk", BooleanValue (false));

NetDeviceContainer staDevice;
staDevice = wifi.Install (phy, staMac, staNode.Get (0));

InternetStackHelper stack;
stack.Install (apNode);
stack.Install (staNode);

Ipv4AddressHelper address;
address.SetBase ("10.1.1.0", "255.255.255.0");
Ipv4InterfaceContainer apIpInterface;
apIpInterface = address.Assign (apDevice);
Ipv4InterfaceContainer staIpInterface;
staIpInterface = address.Assign (staDevice);

MobilityHelper mobility;

mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
mobility.Install (apNode);
apNode.Get (0)->GetObject<ConstantPositionMobilityModel>()->SetPosition (Vector (50.0, 50.0, 0.0));

mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
"Mode", StringValue ("UNIFORM"),
"Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
"Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
mobility.Install (staNode);

uint16_t port = 9;

UdpServerHelper udpServer (port);
ApplicationContainer serverApps = udpServer.Install (apNode.Get (0));
serverApps.Start (Seconds (0.0));
serverApps.Stop (Seconds (12.0));

UdpClientHelper udpClient (apIpInterface.GetAddress (0), port);
udpClient.SetAttribute ("MaxPackets", UintegerValue (10));
udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
ApplicationContainer clientApps = udpClient.Install (staNode.Get (0));
clientApps.Start (Seconds (1.0));
clientApps.Stop (Seconds (12.0));

Simulator::Stop (Seconds (12.0));

Simulator::Run ();
Simulator::Destroy ();

return 0;
}