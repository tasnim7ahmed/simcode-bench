#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaNamedObjectsExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("CsmaNamedObjectsExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // 1. Create four nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Assign human-readable names to nodes immediately after creation
  Ptr<Node> clientNode = nodes.Get (0);
  Ptr<Node> serverNode = nodes.Get (1);
  Ptr<Node> router1Node = nodes.Get (2);
  Ptr<Node> router2Node = nodes.Get (3);

  Names::Add ("client", clientNode);
  Names::Add ("server", serverNode);
  Names::Add ("router1", router1Node);
  Names::Add ("router2", router2Node);

  NS_LOG_INFO ("Initial Node Names:");
  NS_LOG_INFO ("Node ID " << clientNode->GetId () << " is named 'client'");
  NS_LOG_INFO ("Node ID " << serverNode->GetId () << " is named 'server'");
  NS_LOG_INFO ("Node ID " << router1Node->GetId () << " is named 'router1'");
  NS_LOG_INFO ("Node ID " << router2Node->GetId () << " is named 'router2'");

  // Demonstrate renaming nodes in the name system
  NS_LOG_INFO ("\nDemonstrating Node Renaming:");
  NS_LOG_INFO ("Attempting to rename 'router1' to 'intermediate-node-A'.");
  Names::Rename ("router1", "intermediate-node-A");
  NS_LOG_INFO ("Node ID " << Names::Find<Node> ("intermediate-node-A")->GetId () << " is now named 'intermediate-node-A'");
  if (Names::Find<Node> ("router1") == nullptr)
    {
      NS_LOG_INFO ("Old name 'router1' is no longer valid for a Node.");
    }
  else
    {
      NS_LOG_ERROR ("Old name 'router1' is still valid, renaming failed.");
    }

  // Connect nodes with a CSMA LAN
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("20ms"));

  NetDeviceContainer devices = csma.Install (nodes);

  // Assign human-readable names to devices
  Names::Add ("client-eth0", devices.Get (0));
  Names::Add ("server-eth0", devices.Get (1));
  // Note: router1 is now intermediate-node-A
  Names::Add ("intermediate-node-A-eth0", devices.Get (2));
  Names::Add ("router2-eth0", devices.Get (3));

  NS_LOG_INFO ("\nNamed NetDevices:");
  NS_LOG_INFO ("Device ID " << devices.Get (0)->GetIfIndex () << " on Node " << devices.Get (0)->GetNode ()->GetId () << " is named 'client-eth0'");
  NS_LOG_INFO ("Device ID " << devices.Get (1)->GetIfIndex () << " on Node " << devices.Get (1)->GetNode ()->GetId () << " is named 'server-eth0'");
  NS_LOG_INFO ("Device ID " << devices.Get (2)->GetIfIndex () << " on Node " << devices.Get (2)->GetNode ()->GetId () << " is named 'intermediate-node-A-eth0'");
  NS_LOG_INFO ("Device ID " << devices.Get (3)->GetIfIndex () << " on Node " << devices.Get (3)->GetNode ()->GetId () << " is named 'router2-eth0'");

  // Configure attributes of CSMA devices using the Object Names service
  // and mix object names and node attributes in the configuration system.
  NS_LOG_INFO ("\nConfiguring CSMA Device Attributes via Config System:");
  // Set a specific Mtu for all CsmaNetDevice instances via default attribute path
  Config::SetDefault ("ns3::CsmaNetDevice::Mtu", UintegerValue (1500));
  NS_LOG_INFO ("Set default Mtu for all CsmaNetDevice to 1500.");

  // Set DataRate for 'client-eth0' using its object name
  // Note: Client device is on Node 0, Device 0.
  Config::Set ("/Names/client-eth0/DataRate", StringValue ("50Mbps"));
  NS_LOG_INFO ("Set DataRate for 'client-eth0' (Node 0) to 50Mbps using its object name.");

  // Set DataRate for 'server-eth0' using a NodeList path (mixing naming styles)
  // Note: Server device is on Node 1, Device 0.
  Config::Set ("/NodeList/1/DeviceList/0/$ns3::CsmaNetDevice/DataRate", StringValue ("75Mbps"));
  NS_LOG_INFO ("Set DataRate for server's device (Node 1, Device 0) to 75Mbps using NodeList path.");

  // Install internet stack on all nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP echo server on the named "server" node
  UdpEchoServerHelper echoServer (9); // Port 9
  ApplicationContainer serverApps = echoServer.Install (serverNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP echo client on the named "client" node
  // The server's IP address is assigned to the device at index 1 of the interfaces container.
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (clientNode);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable packet tracing and use pcap files
  csma.EnablePcapAll ("csma-echo-named-objects");

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (11.0)); // Stop after client finishes
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}