#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer clientNodes;
  clientNodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer clientDevices = wifi.Install (phy, mac, clientNodes);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (clientNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer clientInterfaces = address.Assign (clientDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (apInterface.GetAddress (0), port));

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (apNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < clientNodes.GetN (); ++i)
    {
      clientApps.Add (clientHelper.Install (clientNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (clientNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (10.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}