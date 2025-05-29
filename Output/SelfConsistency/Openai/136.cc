#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoSimulation");

int
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 5;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of echo packet (bytes)", packetSize);
  cmd.AddValue ("numPackets", "Number of echo packets", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install network stacks
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Echo Server on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on node 0 -> node 1
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}