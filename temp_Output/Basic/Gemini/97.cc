#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaNames");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::GetComponent ("UdpEchoClientApplication"), LOG_LEVEL_INFO);
  LogComponent::SetLevel (LogComponent::GetComponent ("PacketSink"), LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  Names::Add ("client", nodes.Get (0));
  Names::Add ("server", nodes.Get (1));
  Names::Add ("node2", nodes.Get (2));
  Names::Add ("node3", nodes.Get (3));

  Names::Add ("n0", nodes.Get (0));

  Names::Set ("n0", "Name", StringValue ("client"));
  Names::Set ("client", "Name", StringValue ("really-client"));
  Names::Set ("client", "Name", StringValue ("client"));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (500)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (Names::Find<Node> ("server"));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (Names::Find<Node> ("client"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  csma.EnablePcapAll ("csma-names");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}