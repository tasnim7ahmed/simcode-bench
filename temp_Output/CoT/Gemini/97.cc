#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface.h"
#include "ns3/object.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaNames");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetAttribute ("UdpEchoClientApplication", "PacketSize", UintegerValue (1024));
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer nodes;
  nodes.Create (4);

  Names::Add ("Client", nodes.Get (0));
  Names::Add ("Server", nodes.Get (3));

  Names::Add ("node1", nodes.Get (1));
  Names::Add ("node2", nodes.Get (2));

  Names::SetName (nodes.Get (1), "router1");

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (Names::Find<Node> ("Server"));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (3), 9);
  ApplicationContainer clientApps = echoClient.Install (Names::Find<Node> ("Client"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  csma.EnablePcap ("csma-names", devices.Get (0), true);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}