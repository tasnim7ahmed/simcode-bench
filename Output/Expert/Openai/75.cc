#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

void
RxCallback (Ptr<const Packet> packet, const Address &address)
{
  // Empty callback for tracing receptions
}

int main(int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer n0n2, n1n2, csmaNodes, n5n6;
  n0n2.Create(2);  // n0 = n0n2.Get(0), n2 = n0n2.Get(1)
  n1n2.Create(2);  // n1 = n1n2.Get(0), n2b = n1n2.Get(1)
  csmaNodes.Add(n0n2.Get(1)); // n2
  csmaNodes.Create(3);        // n3, n4, n5
  n5n6.Create(2);             // n5b = n5n6.Get(0), n6 = n5n6.Get(1)

  // Naming nodes for clarity
  Ptr<Node> n0 = n0n2.Get(0);
  Ptr<Node> n1 = n1n2.Get(0);
  Ptr<Node> n2 = n0n2.Get(1); // n2 from both n0n2 and n1n2
  Ptr<Node> n3 = csmaNodes.Get(1);
  Ptr<Node> n4 = csmaNodes.Get(2);
  Ptr<Node> n5 = csmaNodes.Get(3); // csmaNodes.Get(3) == n5
  Ptr<Node> n6 = n5n6.Get(1);

  // Point-to-point n0 <-> n2
  PointToPointHelper p2p0;
  p2p0.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p0.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d0 = p2p0.Install (n0, n2);

  // Point-to-point n1 <-> n2
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d1 = p2p1.Install (n1, n2);

  // CSMA (n2, n3, n4, n5)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  // Point-to-point n5 <-> n6
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d2 = p2p2.Install (n5, n6);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (n0);
  stack.Install (n1);
  stack.Install (n2);
  stack.Install (n3);
  stack.Install (n4);
  stack.Install (n5);
  stack.Install (n6);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = address.Assign (d0);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = address.Assign (d1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = address.Assign (csmaDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = address.Assign (d2);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up UDP traffic (CBR) from n0 to n6
  uint16_t port = 8000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (n6);
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (11.0));

  UdpClientHelper client (i3.GetAddress (1), port); // n6 address
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.00375))); // 210B at 448Kb/s: (210*8)/448000 = 0.00375s
  client.SetAttribute ("PacketSize", UintegerValue (210));

  ApplicationContainer clientApp = client.Install (n0);
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (11.0));

  // Enable queue tracing for each point-to-point and CSMA device
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("network-trace.tr");

  // Trace TX and RX queues for p2p and csma devices
  p2p0.EnableAsciiAll (stream);
  p2p1.EnableAsciiAll (stream);
  p2p2.EnableAsciiAll (stream);
  csma.EnableAsciiAll (stream);

  // Trace packet receptions at n6
  Ptr<NetDevice> sinkDevice = n6->GetDevice (0); // Only device on n6
  sinkDevice->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));

  // Also trace packet receptions at UdpServer
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string(n6->GetId()) + "/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}