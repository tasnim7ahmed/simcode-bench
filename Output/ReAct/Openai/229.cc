#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
RxCallback (Ptr<const Packet> packet, const Address &address)
{
  std::cout << "Server received " << packet->GetSize () << " bytes at " 
            << Simulator::Now ().GetSeconds () << "s" << std::endl;
}

int
main (int argc, char *argv[])
{
  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Create UDP server (node 1)
  uint16_t serverPort = 4000;
  UdpServerHelper serverHelper (serverPort);
  ApplicationContainer serverApp = serverHelper.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (2.0));

  // Connect custom logging callback
  serverApp.Get (0)->GetObject<UdpServer> ()->TraceConnectWithoutContext (
    "Rx", MakeCallback (&RxCallback));

  // Create UDP client (node 0)
  UdpClientHelper clientHelper (interfaces.GetAddress (1), serverPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (16));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (2.0));

  Simulator::Stop (Seconds (2.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}