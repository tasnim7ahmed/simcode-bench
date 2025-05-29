#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/object-names.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  // Assign human-readable names to nodes
  Names::Add ("node0", nodes.Get (0));
  Names::Add ("node1", nodes.Get (1));
  Names::Add ("node2", nodes.Get (2));
  Names::Add ("node3", nodes.Get (3));
  Names::Rename ("node0", "client"); // rename node0 to client
  Names::Rename ("node3", "server"); // rename node3 to server

  // Create a CSMA LAN
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices = csma.Install (nodes);

  // Use Object Names service to name devices
  Names::Add ("client/csmaDev", csmaDevices.Get (0));
  Names::Add ("node1/csmaDev", csmaDevices.Get (1));
  Names::Add ("node2/csmaDev", csmaDevices.Get (2));
  Names::Add ("server/csmaDev", csmaDevices.Get (3));

  // Use configuration system: mix object names and node attributes
  Config::Set ("/Names/server/csmaDev/TxQueue/MaxPackets", UintegerValue (300));
  Config::Set ("/Names/client/csmaDev/TxQueue/MaxPackets", UintegerValue (300));
  Config::Set ("/Names/client/$ns3::CsmaNetDevice/Mtu", UintegerValue (1400));
  Config::Set ("/NodeList/2/$ns3::CsmaNetDevice/Mtu", UintegerValue (1450));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (csmaDevices);

  // Install UDP echo server on "server"
  Ptr<Node> serverNode = Names::Find<Node> ("server");
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (serverNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP echo client on "client"
  Ptr<Node> clientNode = Names::Find<Node> ("client");
  Ipv4Address serverAddress = interfaces.GetAddress (3);
  UdpEchoClientHelper echoClient (serverAddress, echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (32));
  ApplicationContainer clientApps = echoClient.Install (clientNode);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing
  csma.EnablePcapAll ("object-names");

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}