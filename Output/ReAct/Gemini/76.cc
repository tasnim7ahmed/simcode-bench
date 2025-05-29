#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedNetwork");

int main (int argc, char *argv[])
{
  bool verbose = true;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (3);

  NodeContainer csmaNodes;
  csmaNodes.Create (4);

  NodeContainer allNodes;
  allNodes.Add (p2pNodes);
  allNodes.Add (csmaNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices02;
  p2pDevices02 = pointToPoint.Install (p2pNodes.Get (0), p2pNodes.Get (2));

  NetDeviceContainer p2pDevices12;
  p2pDevices12 = pointToPoint.Install (p2pNodes.Get (1), p2pNodes.Get (2));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("3ms"));

  NodeContainer csmaSubNodes;
  csmaSubNodes.Add (p2pNodes.Get (2));
  csmaSubNodes.Add (csmaNodes);

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaSubNodes);

  NetDeviceContainer p2pDevices56;
  p2pDevices56 = pointToPoint.Install (csmaNodes.Get (3), p2pNodes.Get (2+3));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02;
  p2pInterfaces02 = address.Assign (p2pDevices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces12;
  p2pInterfaces12 = address.Assign (p2pDevices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56;
  p2pInterfaces56 = address.Assign (p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (p2pNodes.Get (2+3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (p2pInterfaces56.GetAddress (1), 9)));
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (50));
  onoff.SetAttribute ("DataRate", StringValue ("300bps"));
  ApplicationContainer clientApps = onoff.Install (p2pNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("mixedNetwork");
      csma.EnablePcapAll ("mixedNetwork", false);
    }

  Simulator::Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("mixedNetwork.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}