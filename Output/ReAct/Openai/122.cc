#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install NetDevices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Server on node 1
  uint16_t udpPort = 4000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Client on node 0
  uint32_t maxPacketCount = 10;
  Time interPacketInterval = Seconds (1.0);
  UdpClientHelper udpClient (interfaces.GetAddress (1), udpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable Packet Tracing
  pointToPoint.EnablePcapAll ("point-to-point");

  // NetAnim setup
  AnimationInterface anim ("point-to-point.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 20.0);
  anim.SetConstantPosition (nodes.Get (1), 50.0, 20.0);
  anim.EnablePacketMetadata (true);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}