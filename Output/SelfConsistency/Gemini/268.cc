#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaLanSimulation");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (10));

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Create CSMA switch
  NodeContainer switchNode;
  switchNode.Create (1);

  // Create channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (6.56)));

  // Create switch ports
  NetDeviceContainer switchDevices;
  for (int i = 0; i < 4; ++i)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (nodes.Get (i), switchNode.Get (0)));
      switchDevices.Add (link.Get (0));
      switchDevices.Add (link.Get (1));
    }

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);
  internet.Install (switchNode);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (switchDevices);

  // Configure UDP echo server on node 3
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Configure UDP echo client on node 0
  UdpEchoClientHelper echoClient (interfaces.GetAddress (7), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}