#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TreeTopology");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Packet::Drop", StringValue ("*"));

  NodeContainer rootNode;
  rootNode.Create (1);
  NodeContainer childNode1, childNode2, leafNode;
  childNode1.Create (1);
  childNode2.Create (1);
  leafNode.Create (1);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer rootChild1Devices = pointToPoint.Install (rootNode.Get (0), childNode1.Get (0));
  NetDeviceContainer rootChild2Devices = pointToPoint.Install (rootNode.Get (0), childNode2.Get (0));
  NetDeviceContainer child1LeafDevices = pointToPoint.Install (childNode1.Get (0), leafNode.Get (0));

  InternetStackHelper stack;
  stack.Install (rootNode);
  stack.Install (childNode1);
  stack.Install (childNode2);
  stack.Install (leafNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer rootChild1Interfaces = address.Assign (rootChild1Devices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer rootChild2Interfaces = address.Assign (rootChild2Devices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer child1LeafInterfaces = address.Assign (child1LeafDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (rootNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (rootChild1Interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (leafNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}