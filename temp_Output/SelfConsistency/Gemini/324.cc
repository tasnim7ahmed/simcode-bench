#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/http-server.h"
#include "ns3/http-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHttpExample");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (WimaxHelper::GetTypeId ().GetName (), LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (2);

  // Set up Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (1.0)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

  // Set up mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set up the Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer::Create (apDevices, staDevices));

  // Set up HTTP server on node 0
  HttpServerHelper serverHelper (80);
  ApplicationContainer serverApp = serverHelper.Install (nodes.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Set up HTTP client on node 1
  HttpClientHelper clientHelper ("10.1.1.1", 80);
  clientHelper.SetAttribute ("NumOfRequests", UintegerValue (1));
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (1));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}