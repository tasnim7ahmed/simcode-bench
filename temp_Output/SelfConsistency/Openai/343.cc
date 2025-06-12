#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  // Enable logging (optional)
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure the point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install UDP echo server on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP echo client on node 0
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds (1.0);
  uint32_t packetSize = 1024;

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Run the simulator
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}