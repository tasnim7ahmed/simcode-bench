#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterference");

int main (int argc, char *argv[])
{
  bool verbose = false;
  std::string phyMode ("DsssRate11Mbps");
  double primaryRs = -80; // dBm
  double interferenceRs = -80; // dBm
  double timeOffset = 0.000001;
  uint32_t packetSizePrimary = 1000;
  uint32_t packetSizeInterference = 1000;
  std::string tracing = "interference.pcap";
  bool enableTracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("primaryRs", "Rx Power of primary signal (dBm)", primaryRs);
  cmd.AddValue ("interferenceRs", "Rx Power of interfering signal (dBm)", interferenceRs);
  cmd.AddValue ("timeOffset", "Time offset between primary and interfering signals (seconds)", timeOffset);
  cmd.AddValue ("packetSizePrimary", "Size of primary packets", packetSizePrimary);
  cmd.AddValue ("packetSizeInterference", "Size of interfering packets", packetSizeInterference);
  cmd.AddValue ("tracing", "Pcap trace file name", tracing);
  cmd.AddValue ("enableTracing", "Enable pcap tracing", enableTracing);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("wifi-default");
  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSizePrimary));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<ConstantPositionMobilityModel> mobility0 = nodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> mobility1 = nodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> mobility2 = nodes.Get (2)->GetObject<ConstantPositionMobilityModel> ();

  if (mobility0 && mobility1 && mobility2)
    {
      double dist = 20;
      mobility0->SetPosition (Vector (0.0, 0.0, 0.0));
      mobility1->SetPosition (Vector (dist, 0.0, 0.0));
      mobility2->SetPosition (Vector (dist/2, dist, 0.0));
    }

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (nodes.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (interfaces.GetAddress (1), 9);
  sink->Bind (local);

  Simulator::Schedule (Seconds (2.0 + timeOffset), [&]() {
      Ptr<Packet> packet = Create<Packet> (packetSizeInterference);
      sink->SendTo (packet, 0, InetSocketAddress (interfaces.GetAddress (1), 9));
  });

  if(enableTracing)
  {
    wifiPhy.EnablePcap (tracing, devices);
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}