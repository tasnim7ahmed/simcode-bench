#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes02;
  p2pNodes02.Create (2);

  NodeContainer p2pNodes56;
  p2pNodes56.Create (2);

  NodeContainer csmaNodes;
  csmaNodes.Create (4);

  NodeContainer allNodes;
  allNodes.Add (p2pNodes02.Get (0)); // n0
  allNodes.Add (p2pNodes02.Get (1)); // n2 (will be added to csma later)
  allNodes.Add (csmaNodes.Get (0));  // n3
  allNodes.Add (csmaNodes.Get (1));  // n4
  allNodes.Add (csmaNodes.Get (2));  // n5 (will be added to p2p later)
  allNodes.Add (p2pNodes56.Get (1)); // n6
  allNodes.Add (csmaNodes.Get (3));  // n2, again

  NodeContainer csmaPlusOneNode;
  csmaPlusOneNode.Add (csmaNodes.Get (0));
  csmaPlusOneNode.Add (csmaNodes.Get (1));
  csmaPlusOneNode.Add (csmaNodes.Get (2));
  csmaPlusOneNode.Add (p2pNodes02.Get (1));

  NodeContainer csma;
  csma.Add (csmaPlusOneNode);
  csma.Create (0);

  NodeContainer p2p02;
  p2p02.Add (p2pNodes02);
  p2p02.Create (0);

  NodeContainer p2p56;
  p2p56.Add (p2pNodes56);
  p2p56.Create (0);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices02;
  p2pDevices02 = pointToPoint.Install (p2pNodes02);

  NetDeviceContainer p2pDevices56;
  p2pDevices56 = pointToPoint.Install (p2pNodes56);

  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaHelper.SetChannelAttribute ("Delay", StringValue ("20us"));

  NetDeviceContainer csmaDevices;
  csmaDevices = csmaHelper.Install (csmaPlusOneNode);

  InternetStackHelper stack;
  stack.Install (allNodes);

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

  UdpServerHelper server (port);
  server.SetAttribute ("Port", UintegerValue (port));
  ApplicationContainer apps = server.Install (p2pNodes56.Get (1)); //n6

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  uint32_t packetSize = 210;
  DataRate dataRate ("448Kbps");
  Time interPacketInterval = Seconds ((packetSize * 8) / (double) dataRate.GetBitRate ());
  UdpClientHelper client (p2pInterfaces56.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  apps = client.Install (p2pNodes02.Get (0)); //n0
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  // Enable Tracing
  csmaHelper.EnablePcapAll ("mixed_network");
  pointToPoint.EnablePcapAll ("mixed_network");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}