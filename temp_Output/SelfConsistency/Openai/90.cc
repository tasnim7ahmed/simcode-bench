#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/error-model.h"
#include "ns3/queue.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleErrorModelSimulation");

// Trace file stream
static std::ofstream traceStream;

// Callback for Queue events
void
QueueTraceCallback (std::string context, Ptr<const QueueItem> item)
{
  traceStream << Simulator::Now ().GetSeconds ()
              << " " << context << " ENQUEUE UID " << item->GetPacket ()->GetUid ()
              << std::endl;
}

void
QueueDequeueTraceCallback (std::string context, Ptr<const QueueItem> item)
{
  traceStream << Simulator::Now ().GetSeconds ()
              << " " << context << " DEQUEUE UID " << item->GetPacket ()->GetUid ()
              << std::endl;
}

void
QueueDropTraceCallback (std::string context, Ptr<const QueueItem> item)
{
  traceStream << Simulator::Now ().GetSeconds ()
              << " " << context << " DROP UID " << item->GetPacket ()->GetUid ()
              << std::endl;
}

// Callback for packet reception at destination
void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &address)
{
  traceStream << Simulator::Now ().GetSeconds ()
              << " RX UID " << packet->GetUid ()
              << " Size " << packet->GetSize ()
              << std::endl;
}

int
main (int argc, char *argv[])
{
  // Create nodes
  NodeContainer n0n2, n1n2, n2n3;
  NodeContainer nodes;
  nodes.Create (4);
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> n2 = nodes.Get (2);
  Ptr<Node> n3 = nodes.Get (3);
  n0n2.Add (n0);
  n0n2.Add (n2);
  n1n2.Add (n1);
  n1n2.Add (n2);
  n2n3.Add (n2);
  n2n3.Add (n3);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Point-to-Point helpers
  PointToPointHelper p2p_n0n2, p2p_n1n2, p2p_n2n3;
  p2p_n0n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n0n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n0n2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  p2p_n1n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n1n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n1n2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  p2p_n2n3.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_n2n3.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p_n2n3.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  // Install devices and assign error models
  NetDeviceContainer d_n0n2 = p2p_n0n2.Install (n0n2);
  NetDeviceContainer d_n1n2 = p2p_n1n2.Install (n1n2);
  NetDeviceContainer d_n2n3 = p2p_n2n3.Install (n2n3);

  // Apply error models
  // a) Rate error model on n2->n3 direction (device 0 of d_n2n3)
  Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel> ();
  rateError->SetAttribute ("ErrorRate", DoubleValue (0.001));
  d_n2n3.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (rateError));

  // b) Burst error model on n1->n2 direction (device 1 of d_n1n2)
  Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel> ();
  burstError->SetAttribute ("ErrorRate", DoubleValue (0.01));
  burstError->SetAttribute ("BurstSize", UintegerValue (2));
  d_n1n2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (burstError));

  // c) List error model on n0->n2 direction (device 1 of d_n0n2)
  Ptr<ListErrorModel> listError = CreateObject<ListErrorModel> ();
  std::list<uint32_t> errorList;
  errorList.push_back (11);
  errorList.push_back (17);
  listError->SetList (errorList);
  d_n0n2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (listError));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n0n2 = ipv4.Assign (d_n0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n1n2 = ipv4.Assign (d_n1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n2n3 = ipv4.Assign (d_n2n3);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Open trace file
  traceStream.open ("simple-error-model.tr");

  // Trace DropTail queue events on all net devices
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue",
                   MakeBoundCallback (&QueueTraceCallback, "QUEUE"));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Dequeue",
                   MakeBoundCallback (&QueueDequeueTraceCallback, "QUEUE"));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Drop",
                   MakeBoundCallback (&QueueDropTraceCallback, "QUEUE"));

  // UDP CBR flow: n0 -> n3
  uint16_t udpPortA = 4000;
  OnOffHelper udpA ("ns3::UdpSocketFactory",
      InetSocketAddress (i_n2n3.GetAddress (1), udpPortA));
  udpA.SetAttribute ("DataRate", DataRateValue (DataRate ("448Kbps")));
  udpA.SetAttribute ("PacketSize", UintegerValue (210));
  udpA.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  udpA.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer appA = udpA.Install (n0);
  appA.Start (Seconds (0.0));
  appA.Stop (Seconds (2.0));

  PacketSinkHelper sinkA ("ns3::UdpSocketFactory",
      InetSocketAddress (Ipv4Address::GetAny (), udpPortA));
  ApplicationContainer sinkAppA = sinkA.Install (n3);
  sinkAppA.Start (Seconds (0.0));
  sinkAppA.Stop (Seconds (2.0));
  // Trace packet receptions at n3
  Ptr<Application> udpSinkAppA = sinkAppA.Get (0);
  udpSinkAppA->GetObject<PacketSink>()->TraceConnectWithoutContext (
    "Rx", MakeCallback (&PacketReceiveCallback));

  // UDP CBR flow: n3 -> n1
  uint16_t udpPortB = 4001;
  OnOffHelper udpB ("ns3::UdpSocketFactory",
      InetSocketAddress (i_n1n2.GetAddress (0), udpPortB));
  udpB.SetAttribute ("DataRate", DataRateValue (DataRate ("448Kbps")));
  udpB.SetAttribute ("PacketSize", UintegerValue (210));
  udpB.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  udpB.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer appB = udpB.Install (n3);
  appB.Start (Seconds (0.01));
  appB.Stop (Seconds (2.0));

  PacketSinkHelper sinkB ("ns3::UdpSocketFactory",
      InetSocketAddress (Ipv4Address::GetAny (), udpPortB));
  ApplicationContainer sinkAppB = sinkB.Install (n1);
  sinkAppB.Start (Seconds (0.0));
  sinkAppB.Stop (Seconds (2.0));
  // Trace packet receptions at n1
  Ptr<Application> udpSinkAppB = sinkAppB.Get (0);
  udpSinkAppB->GetObject<PacketSink>()->TraceConnectWithoutContext (
    "Rx", MakeCallback (&PacketReceiveCallback));

  // FTP/TCP flow, n0 -> n3
  uint16_t tcpPort = 5000;
  BulkSendHelper ftp ("ns3::TcpSocketFactory",
      InetSocketAddress (i_n2n3.GetAddress (1), tcpPort));
  ftp.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer ftpApp = ftp.Install (n0);
  ftpApp.Start (Seconds (1.2));
  ftpApp.Stop (Seconds (1.35));

  PacketSinkHelper ftpSink ("ns3::TcpSocketFactory",
      InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  ApplicationContainer ftpSinkApp = ftpSink.Install (n3);
  ftpSinkApp.Start (Seconds (0.0));
  ftpSinkApp.Stop (Seconds (2.0));
  // Trace packet receptions at n3
  Ptr<Application> tcpSinkApp = ftpSinkApp.Get (0);
  tcpSinkApp->GetObject<PacketSink>()->TraceConnectWithoutContext (
    "Rx", MakeCallback (&PacketReceiveCallback));

  // Enable checksum for error models
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Run simulation
  Simulator::Stop (Seconds (2.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // Close trace file
  traceStream.close ();

  return 0;
}