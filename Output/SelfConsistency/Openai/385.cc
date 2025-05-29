#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging if needed:
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  Time::SetResolution (Time::NS);

  // Create 2 Nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  // Install point-to-point devices and channels
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up UDP server on Node 1 (port 4000)
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Set up UDP client on Node 0
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01))); // 100 packets per second
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}