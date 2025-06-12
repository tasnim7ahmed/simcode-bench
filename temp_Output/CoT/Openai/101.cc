#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleGlobalRoutingExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Define the point-to-point links
  // Link: n0 <-> n2 (5 Mbps, 2 ms)
  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));
  PointToPointHelper p2p0;
  p2p0.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p0.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p0.SetQueue ("ns3::DropTailQueue<Packet>");

  // Link: n1 <-> n2 (5 Mbps, 2 ms)
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p1.SetQueue ("ns3::DropTailQueue<Packet>");

  // Link: n2 <-> n3 (1.5 Mbps, 10 ms)
  NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p2.SetQueue ("ns3::DropTailQueue<Packet>");

  // Install net devices
  NetDeviceContainer d0d2 = p2p0.Install (n0n2);
  NetDeviceContainer d1d2 = p2p1.Install (n1n2);
  NetDeviceContainer d2d3 = p2p2.Install (n2n3);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign (d0d2);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign (d2d3);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // CBR/UDP from n0 to n3, packet size 210 bytes, rate 448 kbps
  uint16_t cbrPort1 = 5000;
  UdpServerHelper udpServer1 (cbrPort1);
  ApplicationContainer serverApp1 = udpServer1.Install (nodes.Get (3));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (Seconds (10.0));

  UdpClientHelper udpClient1 (i2i3.GetAddress (1), cbrPort1);
  udpClient1.SetAttribute ("MaxPackets", UintegerValue (1000000));
  udpClient1.SetAttribute ("Interval", TimeValue (Seconds (210.0 * 8.0 / 448000.0)));
  udpClient1.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer clientApp1 = udpClient1.Install (nodes.Get (0));
  clientApp1.Start (Seconds (0.1));
  clientApp1.Stop (Seconds (10.0));

  // CBR/UDP from n3 to n1, packet size 210 bytes, rate 448 kbps
  uint16_t cbrPort2 = 5001;
  UdpServerHelper udpServer2 (cbrPort2);
  ApplicationContainer serverApp2 = udpServer2.Install (nodes.Get (1));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (10.0));

  UdpClientHelper udpClient2 (i1i2.GetAddress (0), cbrPort2);
  udpClient2.SetAttribute ("MaxPackets", UintegerValue (1000000));
  udpClient2.SetAttribute ("Interval", TimeValue (Seconds (210.0 * 8.0 / 448000.0)));
  udpClient2.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer clientApp2 = udpClient2.Install (nodes.Get (3));
  clientApp2.Start (Seconds (0.1));
  clientApp2.Stop (Seconds (10.0));

  // FTP/TCP from n0 to n3 (1.2s - 1.35s)
  uint16_t ftpPort = 21;
  Address ftpLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), ftpPort));
  PacketSinkHelper ftpSink ("ns3::TcpSocketFactory", ftpLocalAddress);
  ApplicationContainer ftpSinkApp = ftpSink.Install (nodes.Get (3));
  ftpSinkApp.Start (Seconds (1.1));
  ftpSinkApp.Stop (Seconds (10.0));

  OnOffHelper ftpApp ("ns3::TcpSocketFactory", InetSocketAddress (i2i3.GetAddress (1), ftpPort));
  ftpApp.SetAttribute ("PacketSize", UintegerValue (1448));
  ftpApp.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  ftpApp.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer ftpClientApp = ftpApp.Install (nodes.Get (0));
  ftpClientApp.Start (Seconds (1.2));
  ftpClientApp.Stop (Seconds (1.35));

  // Enable tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("simple-global-routing.tr");
  p2p0.EnableAsciiAll (stream);
  p2p1.EnableAsciiAll (stream);
  p2p2.EnableAsciiAll (stream);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}