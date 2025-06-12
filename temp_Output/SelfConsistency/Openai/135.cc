#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SingleNodeUdpEchoExample");

int 
main (int argc, char *argv[])
{
  // Configure default values and parameters
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 5;
  double interval = 1.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "Number of echo packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (s)", interval);
  cmd.Parse (argc, argv);

  // Create two nodes: node 0 (client and server), node 1 (unused, but needed for p2p)
  NodeContainer nodes;
  nodes.Create (2);

  // Setup point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up UDP echo server on node 0 (same node will be client)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Set up UDP echo client also on node 0 (loopback to self via IP)
  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable logging for packet transmissions and receptions
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}