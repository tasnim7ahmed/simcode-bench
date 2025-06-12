#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetWaveBsmExample");

class BsmApp : public Application
{
public:
  BsmApp () {}
  virtual ~BsmApp () {}
  void Setup (Ptr<Socket> socket, Ipv4Address address, uint16_t port, uint32_t packetSize, Time interval, uint32_t totalPackets);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Ipv4Address     m_peerAddress;
  uint16_t        m_peerPort;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetSize;
  uint32_t        m_packetsSent;
  uint32_t        m_totalPackets;
  Time            m_interval;
};

void
BsmApp::Setup (Ptr<Socket> socket, Ipv4Address address, uint16_t port, uint32_t packetSize, Time interval, uint32_t totalPackets)
{
  m_socket = socket;
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_interval = interval;
  m_totalPackets = totalPackets;
}

void
BsmApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (InetSocketAddress (m_peerAddress, m_peerPort));
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
  if (m_packetsSent < m_totalPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &BsmApp::SendPacket, this);
    }
}

static void
ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      // No operation, just drain packet for demonstration.
    }
}

int
main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simTime = 10.0; // seconds
  uint32_t bsmPacketSize = 200; // bytes
  double bsmInterval = 0.1; // s (10 Hz)
  uint16_t bsmPort = 8080;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  QosWaveMacHelper waveMac = QosWaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      positionAlloc->Add (Vector (0.0, i * 10.0, 0.0));
    }
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (vehicles);
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<MobilityModel> mob = vehicles.Get (i)->GetObject<MobilityModel> ();
      Ptr<ConstantVelocityMobilityModel> cvm = DynamicCast<ConstantVelocityMobilityModel> (mob);
      cvm->SetVelocity (Vector (7.0, 0.0, 0.0)); // 7 m/s along x-axis
    }

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install BSM Applications (UDP packets broadcasted)
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  ApplicationContainer bsmApps;
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      // For each sender, broadcast to 255.255.255.255 every interval
      Ptr<Socket> srcSocket = Socket::CreateSocket (vehicles.Get (i), tid);
      // Enable broadcast
      srcSocket->SetAllowBroadcast(true);
      Ptr<BsmApp> app = CreateObject<BsmApp> ();
      app->Setup (srcSocket, Ipv4Address ("255.255.255.255"), bsmPort, bsmPacketSize, Seconds (bsmInterval), static_cast<uint32_t>(simTime / bsmInterval));
      vehicles.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (0.1));
      app->SetStopTime (Seconds (simTime));
      bsmApps.Add (app);

      // Receiver socket on each vehicle
      Ptr<Socket> recvSocket = Socket::CreateSocket (vehicles.Get (i), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), bsmPort);
      recvSocket->Bind (local);
      recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));
    }

  // NetAnim
  AnimationInterface anim ("vanet-wave-bsm.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get (i), "Vehicle" + std::to_string(i+1));
      anim.UpdateNodeColor (vehicles.Get (i), 255, 0, 0); // Red
    }
  anim.EnablePacketMetadata (true);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}