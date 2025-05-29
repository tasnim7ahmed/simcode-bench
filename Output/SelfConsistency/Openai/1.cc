#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  // Set default values for the simulation
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure the point-to-point link with 5 Mbps and 2 ms delay
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack (IP, UDP, etc.)
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses to the devices
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install UDP Echo server app on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo client app on node 0
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 1;
  Time interPacketInterval = Seconds (1.0);

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing on the point-to-point devices
  pointToPoint.EnablePcapAll ("point_to_point_echo");

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}