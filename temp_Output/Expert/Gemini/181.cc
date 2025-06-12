#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-module.h"
#include "ns3/wave-net-device.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wave-helper.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/application.h"
#include "ns3/packet.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimulation");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp();

  void Setup (Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void ScheduleTx (void);

  Address m_peer;
  uint32_t  m_packetSize;
  uint32_t  m_nPackets;
  DataRate  m_dataRate;
  Socket m_socket;
  bool m_running;
  Time m_interPacketInterval;
  uint32_t  m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (nullptr),
    m_running (false),
    m_interPacketInterval (Seconds (1)),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = nullptr;
}

void
MyApp::Setup (Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  ScheduleTx ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

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

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (1.0 / m_dataRate.GetBitRate () * m_packetSize * 8));
      Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantVelocityMobilityModel> cvmm0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm0->SetVelocity(Vector(10, 0, 0));
  Ptr<ConstantVelocityMobilityModel> cvmm1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm1->SetVelocity(Vector(10, 0, 0));

  WaveMacHelper macHelper;
  WavePhyHelper phyHelper = WavePhyHelper::Default ();
  WaveHelper waveHelper;
  waveHelper.AddInterface (nodes, phyHelper, macHelper);

  NetDeviceContainer devices = waveHelper.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (i.GetAddress (1), port));

  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (sinkAddress, 1024, 1000, DataRate ("1Mbps"));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (10.0));

  AnimationInterface anim ("vanet.xml");
  anim.SetConstantPosition (nodes.Get(0), 0, 1);
  anim.SetConstantPosition (nodes.Get(1), 5, 1);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}