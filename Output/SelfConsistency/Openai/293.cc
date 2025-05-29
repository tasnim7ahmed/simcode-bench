#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create UDP server on node 1
  uint16_t serverPort = 4000;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Create UDP client on node 0
  uint32_t maxPackets = 100;
  Time interPacketInterval = Seconds (0.1); // 100 ms
  uint32_t packetSize = 1024;

  UdpClientHelper udpClient (interfaces.GetAddress (1), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = udpClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}