#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhoc");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  double interval = 0.001;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomRectanglePositionAllocator",
                               "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                               "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

  mobility.Install (staNode);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (apInterface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = client.Install (staNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}