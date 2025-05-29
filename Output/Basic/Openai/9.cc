#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyUdpEchoExample");

int 
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer d30 = p2p.Install (nodes.Get (3), nodes.Get (0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer i01, i12, i23, i30;

  // Assign addresses from 192.9.39.0/24
  address.SetBase ("192.9.39.0", "255.255.255.0");
  i01 = address.Assign (d01);
  address.NewNetwork (); // still .0/24, but increment to avoid overlap in next link
  i12 = address.Assign (d12);
  address.NewNetwork ();
  i23 = address.Assign (d23);
  address.NewNetwork ();
  i30 = address.Assign (d30);

  uint16_t port = 9;
  double appInterval = 2.0;
  double simTime = 20.0;

  // Node 0 Server, Node 1 Client
  UdpEchoServerHelper echoServer0 (port);
  ApplicationContainer serverApps0 = echoServer0.Install (nodes.Get (0));
  serverApps0.Start (Seconds (1.0));
  serverApps0.Stop (Seconds (appInterval - 0.001));

  UdpEchoClientHelper echoClient0 (i01.GetAddress (0), port);
  echoClient0.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient0.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient0.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps0 = echoClient0.Install (nodes.Get (1));
  clientApps0.Start (Seconds (1.2));
  clientApps0.Stop (Seconds (appInterval));


  // Node 1 Server, Node 2 Client
  UdpEchoServerHelper echoServer1 (port+1);
  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (1));
  serverApps1.Start (Seconds (appInterval + 1.0));
  serverApps1.Stop (Seconds (2 * appInterval - 0.001));

  UdpEchoClientHelper echoClient1 (i12.GetAddress (0), port+1);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (2));
  clientApps1.Start (Seconds (appInterval + 1.2));
  clientApps1.Stop (Seconds (2 * appInterval));

  // Node 2 Server, Node 3 Client
  UdpEchoServerHelper echoServer2 (port+2);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (2));
  serverApps2.Start (Seconds (2 * appInterval + 1.0));
  serverApps2.Stop (Seconds (3 * appInterval - 0.001));

  UdpEchoClientHelper echoClient2 (i23.GetAddress (0), port+2);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (3));
  clientApps2.Start (Seconds (2 * appInterval + 1.2));
  clientApps2.Stop (Seconds (3 * appInterval));

  // Node 3 Server, Node 0 Client
  UdpEchoServerHelper echoServer3 (port+3);
  ApplicationContainer serverApps3 = echoServer3.Install (nodes.Get (3));
  serverApps3.Start (Seconds (3 * appInterval + 1.0));
  serverApps3.Stop (Seconds (4 * appInterval - 0.001));

  UdpEchoClientHelper echoClient3 (i30.GetAddress (0), port+3);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (0));
  clientApps3.Start (Seconds (3 * appInterval + 1.2));
  clientApps3.Stop (Seconds (4 * appInterval));

  // Set up NetAnim
  AnimationInterface anim ("ring-topology.xml");

  anim.SetConstantPosition (nodes.Get (0), 70, 50);
  anim.SetConstantPosition (nodes.Get (1), 100, 100);
  anim.SetConstantPosition (nodes.Get (2), 70, 150);
  anim.SetConstantPosition (nodes.Get (3), 30, 100);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}