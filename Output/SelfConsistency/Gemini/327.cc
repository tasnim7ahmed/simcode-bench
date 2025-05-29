#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetOlsrExample");

int main (int argc, char *argv[])
{
  bool enable_pcap = false;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable PCAP Tracing", enable_pcap);
  cmd.Parse (argc, argv);

  LogComponent::SetLogLevel (OlsrRoutingProtocol::LOG_DEBUG, LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (6);

  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=360.0]"));
  mobility.Install (nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpClientHelper client (interfaces.GetAddress (5), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  if (enable_pcap)
    {
      wifiPhy.EnablePcapAll ("manet-olsr");
    }

  AnimationInterface anim ("manet-olsr.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (nodes.Get (1), 30.0, 50.0);
  anim.SetConstantPosition (nodes.Get (2), 50.0, 10.0);
  anim.SetConstantPosition (nodes.Get (3), 70.0, 50.0);
  anim.SetConstantPosition (nodes.Get (4), 90.0, 10.0);
  anim.SetConstantPosition (nodes.Get (5), 50.0, 90.0);

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}