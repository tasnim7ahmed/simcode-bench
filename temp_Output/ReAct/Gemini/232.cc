#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLevel ( "UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetLevel ( "UdpServer", LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create (2);
  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

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
               "Ssid", SsidValue (ssid));

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
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeInterface = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (apNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
      Ptr<Node> clientNode = staNodes.Get (i);
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      InetSocketAddress serverAddress (apNodeInterface.GetAddress (0), port);

      BulkSendHelper source ("ns3::TcpSocketFactory", serverAddress);
      source.SetAttribute ("MaxBytes", UintegerValue (1024));
      ApplicationContainer sourceApps = source.Install (clientNode);
      sourceApps.Start (Seconds (2.0 + i * 0.5));
      sourceApps.Stop (Seconds (9.0));
    }

  if (enablePcap)
    {
      phy.EnablePcapAll ("wifi-tcp");
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}