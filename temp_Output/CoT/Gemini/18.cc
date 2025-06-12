#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-server.h"
#include "ns3/packet-socket-client.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

void MoveAp (Ptr<Node> ap)
{
  static double x = 0.0;
  x += 5.0;
  ap->GetObject<MobilityModel> ()->SetPosition (Vector (x, 0.0, 0.0));
  Simulator::Schedule (Seconds (1.0), &MoveAp, ap);
}

int main (int argc, char *argv[])
{
  bool enableTraces = true;

  CommandLine cmd;
  cmd.AddValue ("EnableTraces", "Enable traces [0|1]", enableTraces);
  cmd.Parse (argc, argv);

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

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);
  mobility.Install (staNodes);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  staNodeInterface = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeInterface;
  apNodeInterface = address.Assign (apDevices);

  PacketSocketHelper packetSocketHelper;
  ApplicationContainer serverApps = packetSocketHelper.Install(apNode, 12345);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(44.0));

  PacketSocketClientHelper clientHelper;
  clientHelper.SetRemote(apNodeInterface.GetAddress (0), 12345);
  clientHelper.SetAttribute ("Protocol", StringValue ("ns3::TcpSocketFactory"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("500kbps")));

  ApplicationContainer clientApps = clientHelper.Install (staNodes.Get (0));
  clientApps.Start (Seconds (0.5));
  clientApps.Stop (Seconds (44.0));

  if (enableTraces)
    {
      phy.EnableMacTraces ();
      phy.EnablePhyTraces ();
    }

  Simulator::Schedule (Seconds (1.0), &MoveAp, apNode.Get (0));

  Simulator::Stop (Seconds (44.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}