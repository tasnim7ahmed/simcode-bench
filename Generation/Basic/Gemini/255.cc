#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

NS_LOG_COMPONENT_DEFINE ("WifiAarfStaAp");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("WifiAarfStaAp", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2); 

  Ptr<Node> apNode = nodes.Get (0);
  Ptr<Node> staNode = nodes.Get (1);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  Ptr<GridPositionAllocator> positionAlloc = CreateObject<GridPositionAllocator> ();
  positionAlloc->SetRectangularGrid (1, 2); 
  positionAlloc->SetResolutionX (1.0);     
  positionAlloc->SetResolutionY (0.0);     
  positionAlloc->SetStartingPosition (Vector (0.0, 0.0, 0.0)); 

  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager"); 

  YansWifiChannelHelper channel;
  channel.SetPropagationLoss ("ns3::FriisPropagationLossModel");
  channel.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  phy.SetErrorRateModel ("ns3::YansErrorRateModel");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MilliSeconds (100)));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false)); 
  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0"); 
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  UdpEchoServerHelper echoServer (9); 
  ApplicationContainer serverApps = echoServer.Install (apNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (9.0));

  UdpEchoClientHelper echoClient (apInterface.GetAddress (0), 9); 
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));       
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); 
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));    
  ApplicationContainer clientApps = echoClient.Install (staNode);
  clientApps.Start (Seconds (2.0)); 
  clientApps.Stop (Seconds (3.0));  

  phy.EnablePcap ("wifi-ap", apDevice.Get (0));
  phy.EnablePcap ("wifi-sta", staDevice.Get (0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}