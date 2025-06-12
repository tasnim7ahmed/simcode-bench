#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedTopology");

int main (int argc, char *argv[])
{
  bool tracing = true;
  uint64_t stopTime = 10;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer p2pNodes02;
  p2pNodes02.Create (2);

  NodeContainer p2pNodes56;
  p2pNodes56.Create (2);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes02.Get (1));
  csmaNodes.Create (4);

  PointToPointHelper pointToPoint02;
  pointToPoint02.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint02.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper pointToPoint56;
  pointToPoint56.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint56.SetChannelAttribute ("Delay", StringValue ("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices02;
  p2pDevices02 = pointToPoint02.Install (p2pNodes02);

  NetDeviceContainer p2pDevices56;
  p2pDevices56 = pointToPoint56.Install (p2pNodes56);

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (p2pNodes02.Get (0));
  stack.Install (csmaNodes);
  stack.Install (p2pNodes56.Get (1));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02;
  p2pInterfaces02 = address.Assign (p2pDevices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56;
  p2pInterfaces56 = address.Assign (p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (p2pNodes56.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (stopTime));

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  var->SetAttribute ("Min", DoubleValue (0.0));
  var->SetAttribute ("Max", DoubleValue (1.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", Ipv4Address::ConvertFrom ("10.1.3.2"));
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (210));
  onoff.SetAttribute ("DataRate", StringValue ("448Kbps"));
  ApplicationContainer apps = onoff.Install (p2pNodes02.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (stopTime));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint02.EnableAsciiAll (ascii.CreateFileStream ("mixed-topology.tr"));

      csma.EnableAsciiAll (ascii.CreateFileStream ("mixed-topology-csma.tr"));

      pointToPoint56.EnableAsciiAll (ascii.CreateFileStream ("mixed-topology-p2p56.tr"));

      Simulator::EnablePcapAll ("mixed-topology", false);
    }

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}