/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetWaveVehicleToRsuExample");

// Simple UDP client application sending periodic packets from vehicle to RSU
class PeriodicUdpSender : public Application
{
public:
  PeriodicUdpSender ();
  virtual ~PeriodicUdpSender ();

  void Setup (Address address, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  Time m_interval;
};

PeriodicUdpSender::PeriodicUdpSender ()
  : m_socket (0),
    m_packetSize (0),
    m_nPackets (0),
    m_packetsSent (0),
    m_interval (Seconds (1.0))
{
}

PeriodicUdpSender::~PeriodicUdpSender ()
{
  m_socket = 0;
}

void
PeriodicUdpSender::Setup (Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
PeriodicUdpSender::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Connect (m_peer);
  m_packetsSent = 0;
  SendPacket ();
}

void
PeriodicUdpSender::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
  Simulator::Cancel (m_sendEvent);
}

void
PeriodicUdpSender::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &PeriodicUdpSender::SendPacket, this);
    }
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("VanetWaveVehicleToRsuExample", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numPackets = 20;
  uint32_t packetSize = 200; // bytes
  double interval = 0.5; // seconds between packets
  double simTime = 15.0; // seconds

  // Create vehicle (node 0) and RSU (node 1)
  NodeContainer nodes;
  nodes.Create (2);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // RSU position (static)
  Ptr<ConstantPositionMobilityModel> rsuMobility = nodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  rsuMobility->SetPosition (Vector (100.0, 0.0, 0.0));

  // Vehicle position and movement
  Ptr<ConstantVelocityMobilityModel> vehMobility = CreateObject<ConstantVelocityMobilityModel> ();
  vehMobility->SetPosition (Vector (0.0, 0.0, 0.0));
  vehMobility->SetVelocity (Vector (10.0, 0.0, 0.0)); // 10 m/s along x
  nodes.Get (0)->AggregateObject (vehMobility);

  // Wi-Fi 802.11p (WAVE) physical and MAC layer setup
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP server on RSU (node 1)
  uint16_t udpPort = 4000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // Install UDP sender on vehicle (node 0)
  Ptr<PeriodicUdpSender> app = CreateObject<PeriodicUdpSender> ();
  Address destAddress = InetSocketAddress (interfaces.GetAddress (1), udpPort);
  app->Setup (destAddress, packetSize, numPackets, Seconds (interval));
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (simTime));

  // Enable tracing
  AsciiTraceHelper ascii;
  wavePhy.EnableAsciiAll (ascii.CreateFileStream ("vanet_wave.tr"));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  // Print server statistics
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp.Get (0));
  NS_LOG_INFO ("Received packets: " << server->GetReceived ());

  return 0;
}