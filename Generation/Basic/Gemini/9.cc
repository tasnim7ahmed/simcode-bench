#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

int main (int argc, char *argv[])
{
  // 1. Enable logging for relevant modules
  ns3::LogComponentEnable ("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("PointToPointHelper", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("Ipv4GlobalRoutingHelper", ns3::LOG_LEVEL_INFO);

  // 2. Set simulation time resolution
  ns3::Time::SetResolution (ns3::Time::NS);
  ns3::CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // 3. Create nodes
  ns3::NodeContainer nodes;
  nodes.Create (4); // Four nodes: 0, 1, 2, 3

  // 4. Configure Point-to-Point links
  ns3::PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

  // 5. Install NetDevices and assign IP addresses for each point-to-point link
  // Link 0-1
  ns3::NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  ns3::Ipv4AddressHelper ipv401;
  ipv401.SetBase ("192.9.39.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces01 = ipv401.Assign (devices01);

  // Link 1-2
  ns3::NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  ns3::Ipv4AddressHelper ipv412;
  ipv412.SetBase ("192.9.40.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces12 = ipv412.Assign (devices12);

  // Link 2-3
  ns3::NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  ns3::Ipv4AddressHelper ipv423;
  ipv423.SetBase ("192.9.41.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces23 = ipv423.Assign (devices23);

  // Link 3-0 (closing the ring)
  ns3::NetDeviceContainer devices30 = pointToPoint.Install (nodes.Get (3), nodes.Get (0));
  ns3::Ipv4AddressHelper ipv430;
  ipv430.SetBase ("192.9.42.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces30 = ipv430.Assign (devices30);

  // Install the internet stack on all nodes
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // Enable global routing for inter-subnet communication
  ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 6. UDP Echo Server Applications
  uint16_t port = 9; // Echo port

  // Install UdpEchoServer on all nodes
  ns3::UdpEchoServerHelper echoServer (port);
  ns3::ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (ns3::Seconds (0.0));
  serverApps.Stop (ns3::Seconds (25.0)); // Servers run for most of the simulation

  // 7. UDP Echo Client Applications
  // Each client sends a maximum of 4 packets with 1-second interval
  uint32_t maxPackets = 4;
  ns3::Time packetInterval = ns3::Seconds (1.0);
  uint32_t packetSize = 1024; // Bytes

  ns3::UdpEchoClientHelper echoClient (ns3::Address (), port); // Remote address set later

  echoClient.SetAttribute ("MaxPackets", ns3::UintegerValue (maxPackets));
  echoClient.SetAttribute ("Interval", ns3::TimeValue (packetInterval));
  echoClient.SetAttribute ("PacketSize", ns3::UintegerValue (packetSize));

  ns3::ApplicationContainer clientApps;

  // Client 0 (Node 0) sends to Server on Node 2
  // Node 2's IP on Link 1-2 is interfaces12.GetAddress(1)
  echoClient.SetRemoteAddress (interfaces12.GetAddress (1));
  clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (ns3::Seconds (1.0));
  clientApps.Stop (ns3::Seconds (6.0)); // Runs for 5 seconds (1.0s to 6.0s)

  // Client 1 (Node 1) sends to Server on Node 3
  // Node 3's IP on Link 2-3 is interfaces23.GetAddress(1)
  echoClient.SetRemoteAddress (interfaces23.GetAddress (1));
  clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (ns3::Seconds (7.0));
  clientApps.Stop (ns3::Seconds (12.0)); // Runs for 5 seconds (7.0s to 12.0s)

  // Client 2 (Node 2) sends to Server on Node 0
  // Node 0's IP on Link 0-1 is interfaces01.GetAddress(0)
  echoClient.SetRemoteAddress (interfaces01.GetAddress (0));
  clientApps = echoClient.Install (nodes.Get (2));
  clientApps.Start (ns3::Seconds (13.0));
  clientApps.Stop (ns3::Seconds (18.0)); // Runs for 5 seconds (13.0s to 18.0s)

  // Client 3 (Node 3) sends to Server on Node 1
  // Node 1's IP on Link 1-2 is interfaces12.GetAddress(0)
  echoClient.SetRemoteAddress (interfaces12.GetAddress (0));
  clientApps = echoClient.Install (nodes.Get (3));
  clientApps.Start (ns3::Seconds (19.0));
  clientApps.Stop (ns3::Seconds (24.0)); // Runs for 5 seconds (19.0s to 24.0s)

  // 8. NetAnim Visualization
  ns3::AnimationInterface anim ("ring-udp-echo.xml");
  anim.EnablePacketMetadata (true); // Enable packet metadata for detailed visualization

  // Set node positions for a clear ring layout in NetAnim
  anim.SetConstantPosition (nodes.Get (0), 0.0, 10.0);   // Top
  anim.SetConstantPosition (nodes.Get (1), 10.0, 0.0);   // Right
  anim.SetConstantPosition (nodes.Get (2), 0.0, -10.0);  // Bottom
  anim.SetConstantPosition (nodes.Get (3), -10.0, 0.0);  // Left

  // 9. Run simulation
  ns3::Simulator::Stop (ns3::Seconds (25.0)); // Stop the simulation at 25 seconds
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}