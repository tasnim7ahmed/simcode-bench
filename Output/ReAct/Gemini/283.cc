#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

int main (int argc, char *argv[])
{
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.Parse (argc,argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create (3);

  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
  mobility.Install (staNodes);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.Install (apNode);

  InternetStackHelper internet;
  internet.Install (staNodes);
  internet.Install (apNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeInterface = address.Assign (apDevices);

  UdpServerHelper echoServer (4000);
  ApplicationContainer serverApps = echoServer.Install (apNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (apNodeInterface.GetAddress (0), 4000);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
      clientApps.Add (echoClient.Install (staNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing == true)
    {
      phy.EnablePcap ("ap", apDevices.Get (0));
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}