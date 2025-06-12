#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/queue.h"
#include "ns3/trace-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleErrorModelExample");

class SimpleTracer
{
public:
  SimpleTracer ()
  {
    m_stream.open ("simple-error-model.tr");
  }

  ~SimpleTracer ()
  {
    if (m_stream.is_open ())
      m_stream.close ();
  }

  void
  QueueTrace (std::string context, Ptr<const QueueItem> item)
  {
    if (m_stream.is_open ())
      {
        m_stream << Simulator::Now ().GetSeconds () << " " << context << ": QueueEnqueue UID=" << item->GetPacket ()->GetUid () << std::endl;
      }
  }

  void
  QueueDequeue (std::string context, Ptr<const QueueItem> item)
  {
    if (m_stream.is_open ())
      {
        m_stream << Simulator::Now ().GetSeconds () << " " << context << ": QueueDequeue UID=" << item->GetPacket ()->GetUid () << std::endl;
      }
  }

  void
  QueueDrop (std::string context, Ptr<const QueueItem> item)
  {
    if (m_stream.is_open ())
      {
        m_stream << Simulator::Now ().GetSeconds () << " " << context << ": QueueDrop UID=" << item->GetPacket ()->GetUid () << std::endl;
      }
  }

  void
  PacketReceived (std::string context, Ptr<const Packet> packet, const Address &from)
  {
    if (m_stream.is_open ())
      {
        m_stream << Simulator::Now ().GetSeconds () << " " << context
                 << ": PacketReceived UID=" << packet->GetUid () << std::endl;
      }
  }

private:
  std::ofstream m_stream;
};

void
InstallAllErrorModels (Ptr<NetDevice> dev)
{
  Ptr<RateErrorModel> rem = CreateObject<RateErrorModel> ();
  rem->SetAttribute ("ErrorRate", DoubleValue (0.001));
  rem->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));

  Ptr<BurstErrorModel> bem = CreateObject<BurstErrorModel> ();
  bem->SetAttribute ("ErrorRate", DoubleValue (0.01));
  bem->SetAttribute ("BurstSize", UintegerValue (3)); // burst size (arbitrary choice)

  Ptr<ListErrorModel> lem = CreateObject<ListErrorModel> ();
  std::list<uint32_t> uids;
  uids.push_back (11);
  uids.push_back (17);
  lem->SetList (uids);

  // Chain the error models: packets pass through all three
  rem->SetNext (bem);
  bem->SetNext (lem);
  dev->SetAttribute ("ReceiveErrorModel", PointerValue (rem));
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer n;
  n.Create (4);

  // Aliases for readability
  Ptr<Node> n0 = n.Get (0);
  Ptr<Node> n1 = n.Get (1);
  Ptr<Node> n2 = n.Get (2);
  Ptr<Node> n3 = n.Get (3);

  // P2P config: (n0<->n2), (n1<->n2): 5 Mbps, 2 ms
  PointToPointHelper p2pA;
  p2pA.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2pA.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pA.SetQueue ("ns3::DropTailQueue");

  // (n3<->n2): 1.5 Mbps, 10 ms
  PointToPointHelper p2pB;
  p2pB.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2pB.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2pB.SetQueue ("ns3::DropTailQueue");

  NetDeviceContainer d0n2 = p2pA.Install (n0, n2);
  NetDeviceContainer d1n2 = p2pA.Install (n1, n2);
  NetDeviceContainer d3n2 = p2pB.Install (n3, n2);

  // Install Error Models to all devices (as a demo, add to all but can focus on n2 links if preferred)
  for (uint32_t i = 0; i < d0n2.GetN (); ++i)
    {
      InstallAllErrorModels (d0n2.Get (i));
    }
  for (uint32_t i = 0; i < d1n2.GetN (); ++i)
    {
      InstallAllErrorModels (d1n2.Get (i));
    }
  for (uint32_t i = 0; i < d3n2.GetN (); ++i)
    {
      InstallAllErrorModels (d3n2.Get (i));
    }

  // Internet stack, routing
  InternetStackHelper internet;
  internet.Install (n);

  // Assign IPs
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0n2 = ipv4.Assign (d0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n2 = ipv4.Assign (d1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3n2 = ipv4.Assign (d3n2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Tracing helper
  Ptr<SimpleTracer> tracer = CreateObject<SimpleTracer> ();

  // Queue tracing
  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue",
    MakeCallback (&SimpleTracer::QueueTrace, tracer));
  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Dequeue",
    MakeCallback (&SimpleTracer::QueueDequeue, tracer));
  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Drop",
    MakeCallback (&SimpleTracer::QueueDrop, tracer));

  // UDP/CBR flows: n0->n3 and n3->n1
  uint16_t udpPort1 = 4001;
  uint16_t udpPort2 = 4002;

  ApplicationContainer udpSinkApps, udpClientApps;

  // n3 UDP sink for n0->n3
  UdpServerHelper udpServer1 (udpPort1);
  udpSinkApps.Add(udpServer1.Install (n3));

  // n1 UDP sink for n3->n1
  UdpServerHelper udpServer2 (udpPort2);
  udpSinkApps.Add (udpServer2.Install (n1));

  udpSinkApps.Start (Seconds (0.0));
  udpSinkApps.Stop (Seconds (5.0));

  // UDP CBR client n0->n3
  UdpClientHelper udpClient1 (i3n2.GetAddress (0), udpPort1);
  udpClient1.SetAttribute ("MaxPackets", UintegerValue (10000000));
  udpClient1.SetAttribute ("Interval", TimeValue (Seconds (0.00375)));
  udpClient1.SetAttribute ("PacketSize", UintegerValue (210));
  udpClient1.SetAttribute ("StartTime", TimeValue (Seconds (0.2)));
  udpClient1.SetAttribute ("StopTime", TimeValue (Seconds (5.0)));
  udpClientApps.Add (udpClient1.Install (n0));

  // UDP CBR client n3->n1
  UdpClientHelper udpClient2 (i1n2.GetAddress (0), udpPort2);
  udpClient2.SetAttribute ("MaxPackets", UintegerValue (10000000));
  udpClient2.SetAttribute ("Interval", TimeValue (Seconds (0.00375)));
  udpClient2.SetAttribute ("PacketSize", UintegerValue (210));
  udpClient2.SetAttribute ("StartTime", TimeValue (Seconds (0.25)));
  udpClient2.SetAttribute ("StopTime", TimeValue (Seconds (5.0)));
  udpClientApps.Add (udpClient2.Install (n3));

  // Packet reception tracing for UDP sinks
  Ptr<UdpServer> udpSink1 = DynamicCast<UdpServer>(udpSinkApps.Get (0));
  Ptr<UdpServer> udpSink2 = DynamicCast<UdpServer>(udpSinkApps.Get (1));
  udpSink1->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&SimpleTracer::PacketReceived, tracer));
  udpSink2->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&SimpleTracer::PacketReceived, tracer));

  // TCP/FTP flow: n0->n3, start at 1.2s, stop at 1.35s
  uint16_t tcpPort = 5002;
  Address tcpLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory", tcpLocalAddress);
  ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (n3);
  tcpSinkApp.Start (Seconds (0.0));
  tcpSinkApp.Stop (Seconds (5.0));
  // Trace TCP Sink
  tcpSinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback (&SimpleTracer::PacketReceived, tracer));

  OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (i3n2.GetAddress (0), tcpPort)));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (500));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.2)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (1.35)));
  onoff.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  ApplicationContainer ftpClient = onoff.Install (n0);

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}