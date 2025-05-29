#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRttSimulation");

static void
RttTracer (std::string context, Time rtt)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s " << "RTT: " << rtt);
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (2.0));
  app->SetStopTime (Seconds (10.0));

  Simulator::Schedule (Seconds (0.001), &TcpSocketBase::SetRttEstimatorFilter, ns3TcpSocket, (unsigned short)8, (unsigned short)1);
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::MyApp/Socket/RttEst/Rtt", MakeCallback (&RttTracer));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      NS_LOG_UNCOND ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND ("  Tx Bytes:   " << i->second.txBytes);
      NS_LOG_UNCOND ("  Rx Bytes:   " << i->second.rxBytes);
      NS_LOG_UNCOND ("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps");
    }

  Simulator::Destroy ();
  return 0;
}

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address          m_peer;
  uint32_t       m_packetSize;
  uint32_t       m_maxPackets;
  DataRate         m_dataRate;
  EventId          m_sendEvent;
  bool             m_running;
  uint32_t       m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (nullptr),
    m_peer (),
    m_packetSize (0),
    m_maxPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = nullptr;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t maxPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_maxPackets = maxPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_maxPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / (double) m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}