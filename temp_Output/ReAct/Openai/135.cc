#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SingleNodeUdpEcho");

int 
main (int argc, char *argv[])
{
  // Configuration parameters
  uint32_t packetSize = 1024;
  uint32_t numPackets = 5;
  double interval = 1.0; // seconds
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets", numPackets);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.Parse (argc, argv);

  // Create two nodes but client/server are the same node
  NodeContainer nodes;
  nodes.Create (1);

  // For point-to-point channel, connect node with itself (loopback sim)
  // Special topology: connect node 0 port 0 to node 0 port 1
  NetDeviceContainer devices;
  {
    // Manually connect the node to itself using a point-to-point channel
    Ptr<PointToPointChannel> channel = CreateObject<PointToPointChannel> ();
    Ptr<PointToPointNetDevice> nd0 = CreateObject<PointToPointNetDevice> ();
    Ptr<PointToPointNetDevice> nd1 = CreateObject<PointToPointNetDevice> ();
    nd0->SetAddress (Mac48Address::Allocate ());
    nd1->SetAddress (Mac48Address::Allocate ());
    nd0->SetDataRate (DataRate (dataRate));
    nd1->SetDataRate (DataRate (dataRate));
    nd0->SetInterframeGap (Seconds (0));
    nd1->SetInterframeGap (Seconds (0));
    nd0->Attach (channel);
    nd1->Attach (channel);
    nodes.Get (0)->AddDevice (nd0);
    nodes.Get (0)->AddDevice (nd1);
    channel->SetAttribute ("Delay", StringValue (delay));
    devices.Add (nd0);
    devices.Add (nd1);
  }

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses to both devices
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Echo Server (Listening on 10.1.1.1 by convention)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApp = echoServer.Install (nodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (20.0));

  // UDP Echo Client on same node, destined for own second IP
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = echoClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (20.0));

  // Enable packet logging
  Config::Connect ("/NodeList/0/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback ([] (Ptr<const Packet> pkt) {
    NS_LOG_INFO ("Sent a packet of size: " << pkt->GetSize ());
  }));
  Config::Connect ("/NodeList/0/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback ([] (Ptr<const Packet> pkt, const Address&) {
    NS_LOG_INFO ("Received a packet of size: " << pkt->GetSize ());
  }));

  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}