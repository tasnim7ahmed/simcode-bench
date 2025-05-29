#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeTcpP2P");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "ThreeNodeTcpP2P", LOG_LEVEL_INFO );

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (interfaces12.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->SetAttribute ("CongestionControl", StringValue ("TcpNewReno"));

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkLocalAddress, 1460, 100000, DataRate ("1Mbps"));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (2.0));
  app->SetStopTime (Seconds (10.0));

  Simulator::Schedule (Seconds(0.00001), &MyApp::ConnectionCallback, app, ns3TcpSocket);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  pointToPoint.EnablePcapAll("ThreeNodeTcpP2P");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("ThreeNodeTcpP2P.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}


class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);
  void SetStartTime (Time startTime);
  void SetStopTime (Time stopTime);

  static void ConnectionCallback  (Ptr<MyApp app, Ptr<Socket> socket);
  static void DataSent            (Ptr<MyApp app, Ptr<Socket> socket, uint32_t size);
  static void SocketRecv            (Ptr<MyApp app, Ptr<Socket> socket);
private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_numPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (nullptr),
    m_peer (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = nullptr;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
MyApp::SetStartTime (Time startTime)
{
  m_startTime = startTime;
}

void
MyApp::SetStopTime (Time stopTime)
{
  m_stopTime = stopTime;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
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

void MyApp::ConnectionCallback  (Ptr<MyApp> app, Ptr<Socket> socket)
{
  NS_LOG_INFO ("Connection finished");
  app->ScheduleTx ();
}

void MyApp::DataSent            (Ptr<MyApp> app, Ptr<Socket> socket, uint32_t size)
{
  NS_LOG_INFO ("Data Sent!");
}

void MyApp::SocketRecv            (Ptr<MyApp> app, Ptr<Socket> socket)
{
  NS_LOG_INFO ("Data Received!");
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
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