#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMobile");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 2;

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (nWifi);

  NodeContainer apNode;
  apNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
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

  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobility.Install (staNodes);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  for (uint32_t i = 0; i < nWifi; ++i)
    {
      UdpClientHelper client (apInterfaces.GetAddress (0), 9);
      client.SetAttribute ("MaxPackets", UintegerValue (10));
      client.SetAttribute ("Interval", TimeValue (Seconds (1.)));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApps = client.Install (staNodes.Get (i));
      clientApps.Start (Seconds (2.0 + i * 0.1));
      clientApps.Stop (Seconds (10.0));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}