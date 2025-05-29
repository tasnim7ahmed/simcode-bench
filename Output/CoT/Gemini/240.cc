#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMulticast");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFirstLevel (LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create (3);

  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (1.0)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpClientServerHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpClientServerHelper echoServer (Ipv4Address ("224.0.0.1"), 9);
  ApplicationContainer serverApps = echoServer.Install (staNodes.Get (1));
  serverApps.Add (echoServer.Install (staNodes.Get (2)));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  Ptr<Ipv4> ipv4 = apNode.Get(0)->GetObject<Ipv4>();
  ipv4->SetRoutingProtocol(CreateObject<Ipv4GlobalRouting>());
  ipv4->AddMulticastRoute(Ipv4Address("224.0.0.1"), apInterfaces.GetAddress(0));

  for (uint32_t i = 1; i < staNodes.GetN(); ++i)
  {
      Ptr<Ipv4> staIpv4 = staNodes.Get(i)->GetObject<Ipv4>();
      staIpv4->SetRoutingProtocol(CreateObject<Ipv4GlobalRouting>());
      staIpv4->AddMulticastRoute(Ipv4Address("224.0.0.1"), staInterfaces.GetAddress(i-1));
      staIpv4->JoinMulticastGroup(Ipv4Address("224.0.0.1"), 0);
  }

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}