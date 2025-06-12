#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/on-off-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaNames");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetAttribute ("UdpEchoClientApplication", "PacketSize", UintegerValue (1024));
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1000));

  NodeContainer nodes;
  nodes.Create (4);

  // Assign names to nodes
  Names::Add ("client", nodes.Get (0));
  Names::Add ("server", nodes.Get (3));
  Names::Add ("intermediate1", nodes.Get (1));
  Names::Add ("intermediate2", nodes.Get (2));

  // Rename a node
  Names::Add ("the-real-server", Names::Find<Node> ("server"));
  Names::Remove ("server");

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (Names::Find<Node> ("the-real-server"));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (3), 9);
  ApplicationContainer clientApps = echoClient.Install (Names::Find<Node> ("client"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Configure CSMA attributes using Object Names service
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  Config::Set ("/Names/intermediate1/DeviceList/*/$ns3::CsmaNetDevice/InterframeGap", StringValue("20ns"));

  // Enable packet capture on all devices
  csma.EnablePcapAll ("csma-names");

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}