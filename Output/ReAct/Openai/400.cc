#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  Ipv4InterfaceContainer apInterface;
  staInterface = address.Assign (staDevice);
  apInterface = address.Assign (apDevice);

  uint16_t port = 9;

  Address serverAddress (InetSocketAddress (apInterface.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (wifiApNode.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("81920bps"))); // 1024*8/0.1 = 81920bps
  clientHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));

  ApplicationContainer clientApp = clientHelper.Install (wifiStaNode.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}