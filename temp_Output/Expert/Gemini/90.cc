#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/error-model.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleErrorModel");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogComponentEnable (true);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p_n0_n2;
  p2p_n0_n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n0_n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n0_n2.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  PointToPointHelper p2p_n1_n2;
  p2p_n1_n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n1_n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n1_n2.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  PointToPointHelper p2p_n2_n3;
  p2p_n2_n3.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_n2_n3.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p_n2_n3.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer dev_n0_n2 = p2p_n0_n2.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer dev_n1_n2 = p2p_n1_n2.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer dev_n2_n3 = p2p_n2_n3.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0_n2 = address.Assign (dev_n0_n2);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1_n2 = address.Assign (dev_n1_n2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n2_n3 = address.Assign (dev_n2_n3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port_n0_n3 = 9;
  V4CbrHelper cbrHelper_n0_n3 ("ns3::UdpSocketFactory", InetSocketAddress (if_n2_n3.GetAddress (1), port_n0_n3));
  cbrHelper_n0_n3.SetKeyAttribute ("PacketSize", UintegerValue (210));
  cbrHelper_n0_n3.SetKeyAttribute ("Interval", TimeValue (Seconds (0.00375)));

  ApplicationContainer cbrApp_n0_n3 = cbrHelper_n0_n3.Install (nodes.Get (0));
  cbrApp_n0_n3.Start (Seconds (0.5));
  cbrApp_n0_n3.Stop (Seconds (1.5));

  uint16_t port_n3_n1 = 10;
  V4CbrHelper cbrHelper_n3_n1 ("ns3::UdpSocketFactory", InetSocketAddress (if_n1_n2.GetAddress (0), port_n3_n1));
  cbrHelper_n3_n1.SetKeyAttribute ("PacketSize", UintegerValue (210));
  cbrHelper_n3_n1.SetKeyAttribute ("Interval", TimeValue (Seconds (0.00375)));

  ApplicationContainer cbrApp_n3_n1 = cbrHelper_n3_n1.Install (nodes.Get (3));
  cbrApp_n3_n1.Start (Seconds (0.75));
  cbrApp_n3_n1.Stop (Seconds (1.75));

  BulkSendHelper ftp_n0_n3 ("ns3::TcpSocketFactory", InetSocketAddress (if_n2_n3.GetAddress (1), 11));
  ftp_n0_n3.SetAttribute ("SendSize", UintegerValue (1400));
  ApplicationContainer ftpApp_n0_n3 = ftp_n0_n3.Install (nodes.Get (0));
  ftpApp_n0_n3.Start (Seconds (1.2));
  ftpApp_n0_n3.Stop (Seconds (1.35));

  RateErrorModel errorModel;
  errorModel.SetAttribute ("ErrorRate", DoubleValue (0.001));
  errorModel.SetAttribute ("RanVar", StringValue ("ns3::UniformRandomVariable[Stream=50]"));

  BurstErrorModel burstErrorModel;
  burstErrorModel.SetAttribute ("ErrorRate", DoubleValue (0.01));
  burstErrorModel.SetAttribute ("BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=5]"));
  burstErrorModel.SetAttribute ("RanVar", StringValue ("ns3::UniformRandomVariable[Stream=51]"));

  ListErrorModel listErrorModel;
  listErrorModel.SetAttribute("ErrorRate", DoubleValue(1.0));
  listErrorModel.AddUid(11);
  listErrorModel.AddUid(17);

  dev_n0_n2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (&errorModel));
  dev_n1_n2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (&burstErrorModel));
  dev_n2_n3.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (&listErrorModel));

  Simulator::Stop (Seconds (2.0));

  AsciiTraceHelper ascii;
  p2p_n0_n2.EnableAsciiAll (ascii.CreateFileStream ("simple-error-model.tr"));
  p2p_n1_n2.EnableAsciiAll (ascii.CreateFileStream ("simple-error-model.tr"));
  p2p_n2_n3.EnableAsciiAll (ascii.CreateFileStream ("simple-error-model.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}