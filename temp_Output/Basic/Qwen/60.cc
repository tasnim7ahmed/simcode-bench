#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransfer");

class MyApp : public Application
{
public:
  static TypeId GetTypeId (void);
  MyApp ();
  virtual ~MyApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    .AddAttribute ("PacketSize", "Size of the packets",
                   UintegerValue (512),
                   MakeUintegerAccessor (&MyApp::m_packetSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NumPackets", "Number of packets to send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&MyApp::m_nPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DataRate", "Data rate",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&MyApp::m_dataRate),
                   MakeDataRateChecker ());
  return tid;
}

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

void MyApp::Setup (Ptr<Socket> socket, const Address &address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Connect (InetSocketAddress::ConvertFrom(m_peer));
    }
  else
    {
      m_socket->Connect (m_peer);
    }
  SendPacket ();
}

void MyApp::StopApplication (void)
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

void MyApp::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      Time tNext (Seconds ((double)m_packetSize * 8 / (double)m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

void CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

int main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices01, devices12;
  devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces12.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (2));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  MyApp app;
  app.Setup (ns3TcpSocket, sinkAddress, 1040, 2000000 / 1040 + 1, DataRate ("10Mbps"));
  apps.Add (&app);
  app.SetStartTime (Seconds (1.0));
  app.SetStopTime (Seconds (10.0));

  AsciiTraceHelper asciiTraceHelper;
  pointToPoint.EnableAsciiAll (asciiTraceHelper.CreateFileStream ("tcp-large-transfer.tr"));

  pointToPoint.EnablePcapAll ("tcp-large-transfer");

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}