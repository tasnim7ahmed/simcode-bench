#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer spokeNodes;
  spokeNodes.Create (8);

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create net device containers for each spoke
  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices[8];

  for (int i = 0; i < 8; ++i)
    {
      NetDeviceContainer link = pointToPoint.Install (hubNode.Get (0), spokeNodes.Get (i));
      hubDevices.Add (link.Get (0));
      spokeDevices[i].Add (link.Get (1));
    }

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (hubNode);
  internet.Install (spokeNodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces[8];

  for (int i = 0; i < 8; ++i)
    {
      Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (hubDevices.Get (i), spokeDevices[i].Get (0)));
      hubInterfaces.Add (interfaces.Get (0));
      spokeInterfaces[i].Add (interfaces.Get (1));
      address.NewNetwork ();
    }

  // Create OnOff application (spokes to hub)
  OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (hubInterfaces.GetAddress (0), 50000)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (137));
  onoff.SetAttribute ("DataRate", StringValue ("14kbps"));

  ApplicationContainer onoffApplications[8];
  for (int i = 0; i < 8; ++i)
    {
      onoffApplications[i] = onoff.Install (spokeNodes.Get (i));
      onoffApplications[i].Start (Seconds (1.0 + i * 0.1)); // Staggered start times
      onoffApplications[i].Stop (Seconds (9.0 + i * 0.1));
    }

  // Create PacketSink application (hub)
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 50000));
  ApplicationContainer sinkApp = sink.Install (hubNode.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Enable global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable PCAP Tracing
  pointToPoint.EnablePcapAll ("star");

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}