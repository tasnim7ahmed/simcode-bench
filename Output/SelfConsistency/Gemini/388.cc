#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimple");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  // The underlying MAC layer will perform clear channel assessment
  // to avoid transmissions when the channel is busy.  We can disable
  // this feature to make it easier to cause collisions and
  // capture them with a packet analyzer.
  //Config::SetDefault ("ns3::WifiMac::EDCAQueue::Cwmin", StringValue ("1023"));
  //Config::SetDefault ("ns3::WifiMac::EDCAQueue::Cwmax", StringValue ("1023"));

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

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
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  NodeContainer wifiNodes;
  wifiNodes.Add (wifiStaNodes);
  wifiNodes.Add (wifiApNode);
  mobility.Install (wifiNodes);

  // Set static positions
  Ptr<Node> node0 = wifiNodes.Get (0);
  Ptr<MobilityModel> mobility0 = node0->GetObject<MobilityModel> ();
  mobility0->SetPosition (Vector (0.0, 0.0, 0.0));

  Ptr<Node> node1 = wifiNodes.Get (1);
  Ptr<MobilityModel> mobility1 = node1->GetObject<MobilityModel> ();
  mobility1->SetPosition (Vector (5.0, 5.0, 0.0));

  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);
  address.Assign (staDevices);
  Ipv4InterfaceContainer wifiInterfaces = address.Assign (apDevices);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (wifiApNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (wifiInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (tracing == true)
    {
      phy.EnablePcap ("wifi-simple", apDevices.Get (0));
      pointToPoint.EnablePcapAll ("wifi-simple");
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}