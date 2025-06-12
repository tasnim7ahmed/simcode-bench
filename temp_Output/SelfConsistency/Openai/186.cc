/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * VANET simulation with 5 vehicles using 802.11p (WAVE)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Simple application to send/receive Basic Safety Messages (BSM)
class BsmApp : public Application
{
public:
  BsmApp ();
  virtual ~BsmApp ();
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  uint32_t        m_packetSize;
  Time            m_interval;
  EventId         m_sendEvent;
  bool            m_running;
};

BsmApp::BsmApp ()
  : m_socket (0),
    m_peerAddress (),
    m_packetSize (0),
    m_interval (Seconds (1.0)),
    m_sendEvent (),
    m_running (false)
{
}

BsmApp::~BsmApp ()
{
  m_socket = 0;
}

void 
BsmApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
{
  m_socket = socket;
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_interval = interval;
}

void
BsmApp::StartApplication (void)
{
  m_running = true;
  m_socket->Bind ();
  m_socket->SetRecvCallback (MakeCallback (&BsmApp::HandleRead, this));
  ScheduleTx ();
}

void 
BsmApp::StopApplication (void)
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
BsmApp::ScheduleTx (void)
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &BsmApp::SendPacket, this);
    }
}

void
BsmApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->SendTo (packet, 0, m_peerAddress);
  Simulator::Schedule (m_interval, &BsmApp::SendPacket, this);
}

void
BsmApp::HandleRead (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      if (packet->GetSize () > 0)
        {
          // Optionally log message receipt
          NS_LOG_INFO ("Node " << GetNode ()->GetId () << " received BSM of " << packet->GetSize ()
                        << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
        }
    }
}

int 
main (int argc, char *argv[])
{
  LogComponentEnable ("BsmApp", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numVehicles = 5;
  double simulationTime = 20.0; // seconds
  double txInterval = 1.0; // seconds
  uint32_t bsmSize = 200; // bytes

  // Create nodes
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Set up mobility: vehicles move in a straight line (1D)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      positionAlloc->Add (Vector (i * 30.0, 0.0, 0.0)); // 30m apart
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (15.0, 0.0, 0.0)); // 15 m/s (~54 km/h) along X
    }

  // Set up 802.11p WAVE device
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  YansWaveChannelHelper waveChannel = YansWaveChannelHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (vehicles);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up applications: each vehicle sends BSM on a broadcast address
  uint16_t bsmPort = 5000;

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<Socket> recvSocket = Socket::CreateSocket (vehicles.Get (i), TypeId::LookupByName ("ns3::UdpSocketFactory"));
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), bsmPort);
      recvSocket->Bind (local);
      recvSocket->SetRecvCallback (MakeCallback (&BsmApp::HandleRead, DynamicCast<BsmApp> (vehicles.Get (i)->GetApplication (0)).Get ()));

      Ptr<Socket> sendSocket = Socket::CreateSocket (vehicles.Get (i), TypeId::LookupByName ("ns3::UdpSocketFactory"));
      Address broadcastAddress = InetSocketAddress (Ipv4Address ("255.255.255.255"), bsmPort);

      Ptr<BsmApp> app = CreateObject<BsmApp> ();
      app->Setup (sendSocket, broadcastAddress, bsmSize, Seconds (txInterval));
      vehicles.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i * 0.1)); // slight delay to avoid collisions at t=1.0s
      app->SetStopTime (Seconds (simulationTime));
    }

  // NetAnim setup
  AnimationInterface anim ("vanet.xml");
  anim.SetMaxPktsPerTraceFile (5000);
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get (i), "Vehicle" + std::to_string (i));
      anim.UpdateNodeColor (vehicles.Get (i), 0, 0, 255); // blue
    }
  anim.EnablePacketMetadata ();
  anim.EnableIpv4RouteTracking ("vanet-routes.xml", Seconds (0), Seconds (simulationTime), Seconds (0.25));

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}