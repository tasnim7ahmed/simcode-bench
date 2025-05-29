#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  bool tracing = true;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  double interval = 0.001;
  double simulationTime = 10.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.AddValue ("tracing", "Enable or disable packet tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);

  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (nodes);

  if (tracing)
    {
      phy.EnablePcapAll ("aodv-adhoc");
    }

  AnimationInterface anim ("aodv-animation.xml");
  anim.SetMaxPktsPerTraceFile(500000);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}