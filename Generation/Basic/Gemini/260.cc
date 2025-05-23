#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"

int main (int argc, char *argv[])
{
  double simTime = 10.0;

  NodeContainer nodes;
  nodes.Create (5);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)),
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install (nodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel"); 

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationLossModel ("ns3::FriisPropagationLossModel");
  wifiChannel.SetPropagationDelayModel ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac"); 

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpEchoServerHelper echoServer (9); 
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (4), 9); 
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); 
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0)); 
  clientApps.Stop (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}