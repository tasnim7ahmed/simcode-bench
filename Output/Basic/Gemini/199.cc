#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLanSimulation");

int main (int argc, char *argv[])
{
  bool tracing = false;
  uint64_t simTime = 10;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilterForNextLog (
      "ns3::TcpSocketBase", Logging::LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create (3);

  NodeContainer endNodes;
  endNodes.Create (2);

  NodeContainer lanNodes;
  lanNodes.Create (2);

  InternetStackHelper stack;
  stack.Install (routers);
  stack.Install (endNodes);
  stack.Install (lanNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer r1r2Devs = p2p.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer r2r3Devs = p2p.Install (routers.Get (1), routers.Get (2));
  NetDeviceContainer r3endDev = p2p.Install (routers.Get (2), endNodes.Get (1));
  NetDeviceContainer r1endDev = p2p.Install (routers.Get (0), endNodes.Get (0));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer lanDevices = csma.Install (NodeContainer (lanNodes, routers.Get (1)));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2Ifaces = address.Assign (r1r2Devs);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3Ifaces = address.Assign (r2r3Devs);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer r3endIfaces = address.Assign (r3endDev);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer r1endIfaces = address.Assign (r1endDev);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer lanIfaces = address.Assign (lanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (r3endIfaces.GetAddress (1, 0), port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (lanNodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simTime - 1));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (endNodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  if (tracing == true)
    {
      p2p.EnablePcapAll ("tcp-lan-simulation");
      csma.EnablePcap ("tcp-lan-simulation", lanDevices.Get (0), true);
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}