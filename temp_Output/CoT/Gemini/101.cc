#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleGlobalRouting");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint02;
  pointToPoint02.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint02.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devices02 = pointToPoint02.Install (nodes.Get (0), nodes.Get (2));

  PointToPointHelper pointToPoint12;
  pointToPoint12.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint12.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devices12 = pointToPoint12.Install (nodes.Get (1), nodes.Get (2));

  PointToPointHelper pointToPoint23;
  pointToPoint23.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint23.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer devices23 = pointToPoint23.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign (devices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // CBR/UDP from n0 to n3
  uint16_t portCbr03 = 9;
  OnOffHelper onoff03 ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (interfaces23.GetAddress (1), portCbr03)));
  onoff03.SetConstantRate (DataRate ("448kbps"));
  onoff03.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer apps03 = onoff03.Install (nodes.Get (0));
  apps03.Start (Seconds (1.0));
  apps03.Stop (Seconds (10.0));

  // CBR/UDP from n3 to n1
  uint16_t portCbr31 = 10;
  OnOffHelper onoff31 ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (interfaces12.GetAddress (0), portCbr31)));
  onoff31.SetConstantRate (DataRate ("448kbps"));
  onoff31.SetAttribute ("PacketSize", UintegerValue (210));
  ApplicationContainer apps31 = onoff31.Install (nodes.Get (3));
  apps31.Start (Seconds (1.0));
  apps31.Stop (Seconds (10.0));

  // FTP/TCP from n0 to n3
  uint16_t portFtp03 = 21;
  BulkSendHelper ftp03 ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (interfaces23.GetAddress (1), portFtp03)));
  ftp03.SetAttribute ("SendSize", UintegerValue (1024));
  ApplicationContainer appFtp03 = ftp03.Install (nodes.Get (0));
  appFtp03.Start (Seconds (1.2));
  appFtp03.Stop (Seconds (1.35));

  Simulator::Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  pointToPoint02.EnableAsciiAll (ascii.CreateFileStream ("simple-global-routing.tr"));
  pointToPoint12.EnableAsciiAll (ascii.CreateFileStream ("simple-global-routing.tr"));
  pointToPoint23.EnableAsciiAll (ascii.CreateFileStream ("simple-global-routing.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}