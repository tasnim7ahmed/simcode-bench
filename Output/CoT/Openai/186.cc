#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class BsmApp : public Application
{
public:
  BsmApp ();
  virtual ~BsmApp ();
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_packetsSent;
  EventId         m_sendEvent;
  bool            m_running;
};

BsmApp::BsmApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (100),
    m_nPackets (0),
    m_pktInterval (Seconds (1.0)),
    m_packetsSent (0),
    m_running (false)
{
}

BsmApp::~BsmApp ()
{
  m_socket = 0;
}

void
BsmApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
}

void
BsmApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
BsmApp::StopApplication (void)
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void
BsmApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  ++m_packetsSent;

  if (m_packetsSent < m_nPackets)
    ScheduleTx ();
}

void
BsmApp::ScheduleTx (void)
{
  if (m_running)
  {
    m_sendEvent = Simulator::Schedule (m_pktInterval, &BsmApp::SendPacket, this);
  }
}

NS_LOG_COMPONENT_DEFINE ("VanetBsmWaveExample");

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simTime = 20.0;
  double distance = 50.0;
  uint32_t bsmSize = 200;
  double bsmInterval = 0.1;

  CommandLine cmd;
  cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("distance", "Distance between vehicles (meters)", distance);
  cmd.AddValue ("bsmInterval", "BSM send interval (s)", bsmInterval);
  cmd.AddValue ("bsmSize", "BSM packet size (bytes)", bsmSize);
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Mobility: Each vehicle moves along the X axis at 10 m/s, spaced "distance" apart
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    pos->Add (Vector (i * distance, 0.0, 0.0));
  }
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel> ();
    mob->SetVelocity (Vector (10.0, 0.0, 0.0));
  }

  // 802.11p
  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

  // Internet/IP stack
  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install BSM (broadcast)
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  ApplicationContainer apps;
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    Ptr<Socket> socket = Socket::CreateSocket (vehicles.Get(i), tid);
    // Broadcast address (limited broadcast)
    InetSocketAddress broadcastAddr = InetSocketAddress (Ipv4Address ("255.255.255.255"), 4444);
    socket->SetAllowBroadcast (true);

    Ptr<BsmApp> app = CreateObject<BsmApp> ();
    app->Setup (socket, broadcastAddr, bsmSize, uint32_t(simTime/bsmInterval), Seconds (bsmInterval));
    vehicles.Get(i)->AddApplication (app);
    app->SetStartTime (Seconds (1.0));
    app->SetStopTime (Seconds (simTime));
    apps.Add (app);
  }

  // Install packet receive trace on all nodes
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::SocketApplication/Recv", MakeCallback ([](Ptr<const Packet> packet, const Address &from){
    NS_LOG_UNCOND ("At " << Simulator::Now ().GetSeconds () << "s, received " << packet->GetSize () << " bytes");
  }));

  // Animation
  AnimationInterface anim ("vanet-wavetest.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    std::ostringstream oss;
    oss << "Vehicle-" << i;
    anim.UpdateNodeDescription (vehicles.Get(i), oss.str ());
    anim.UpdateNodeColor (vehicles.Get(i), 0, 255, 0); // green
  }

  Simulator::Stop (Seconds (simTime + 2));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}