#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/rip.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorSplitHorizon");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool printRoutingTables = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables if true.", printRoutingTables);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4RoutingProtocol", LOG_LEVEL_ALL);
    }

  Time::SetResolution (Time::NS);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (3);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  NS_LOG_INFO ("Assign IP Addresses.");
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = ipv4.Assign (devices01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = ipv4.Assign (devices12);

  // Routing configuration
  RipHelper ripRouting;
  ripRouting.ExcludeInterface (nodes.Get (0), 1);  // Disable Split Horizon for Node A to Node B interface (index 1). Normally, we do not exclude the interface.  For demonstrative purposes only.
  ripRouting.ExcludeInterface (nodes.Get (2), 1);  // Disable Split Horizon for Node C to Node B interface (index 1). Normally, we do not exclude the interface.  For demonstrative purposes only.
  Address multicastGroup("224.0.0.9");

  ripRouting.Install (nodes);

  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("rip.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (10.0), routingStream);

  NS_LOG_INFO ("Create Applications.");
  // Create a sink application on node one
  UdpServerHelper sink (9);
  ApplicationContainer sinkApp = sink.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Create a source application to send from node zero to node one
  UdpClientHelper source (interfaces12.GetAddress (1), 9);
  source.SetAttribute ("MaxPackets", UintegerValue (1000));
  source.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  source.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer sourceApp = source.Install (nodes.Get (0));
  sourceApp.Start (Seconds (2.0));
  sourceApp.Stop (Seconds (10.0));


  Simulator::Stop (Seconds (11.0));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}