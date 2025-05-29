#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND ("Packet received at " << Simulator::Now ().GetSeconds () << "s: " << packet->GetSize () << " bytes");
}

void
TxTrace (Ptr<const Packet> packet)
{
  NS_LOG_UNCOND ("Packet sent at " << Simulator::Now ().GetSeconds () << "s: " << packet->GetSize () << " bytes");
}

int
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 5;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (1);

  NetDeviceContainer devices;
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  devices = pointToPoint.Install (nodes.Get (0), nodes.Get (0));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback (&TxTrace));
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback (&RxTrace));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}