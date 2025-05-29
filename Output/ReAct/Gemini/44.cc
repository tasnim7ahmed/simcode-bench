#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleInfrastructure");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1472; // bytes
  uint32_t numPackets = 1000;
  double interval = 0.001; // seconds
  double rss = -50;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("packetSize", "Size of packets sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("rss", "Receive signal strength (dBm)", rss);
  cmd.Parse (argc, argv);

  // Enable logging from the udp client and server
  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer staNodes = NodeContainer (nodes.Get (0));
  NodeContainer apNodes = NodeContainer (nodes.Get (1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("RxGain", DoubleValue (0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

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
  apDevices = wifi.Install (phy, mac, apNodes);

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
  mobility.Install (apNodes);

  FixedRssLossModel fixedRss;
  fixedRss.SetRss (rss);
  phy.AddPropagationLossModel (fixedRss);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (apNodes);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (staNodes);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  phy.EnablePcap ("wifi-simple-infrastructure", apDevices.Get (0));

  AnimationInterface anim ("wifi-simple-infrastructure.xml");
  anim.SetConstantPosition (nodes.Get (0), 1.0, 1.0);
  anim.SetConstantPosition (nodes.Get (1), 10.0, 1.0);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}