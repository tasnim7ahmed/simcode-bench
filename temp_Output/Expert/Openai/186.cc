#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETWaveExample");

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
};

BsmApp::BsmApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (100),
    m_nPackets (0),
    m_pktInterval (Seconds (1)),
    m_packetsSent (0)
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
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
BsmApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
BsmApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
BsmApp::ScheduleTx (void)
{
  Simulator::Schedule (m_pktInterval, &BsmApp::SendPacket, this);
}

int
main (int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simulationTime = 20.0; // seconds
  double nodeSpeed = 20.0; // m/s
  double interval = 1.0; // BSM interval (seconds)
  uint32_t bsmPacketSize = 200;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
  wavePhy.SetChannel (waveChannel.Create ());
  wavePhy.SetPcapDataLinkType (YansWavePhyHelper::DLT_IEEE802_11_RADIO);

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();
  waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));
  NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (50.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (numVehicles),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (nodeSpeed, 0, 0));
    }

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      for (uint32_t j = 0; j < vehicles.GetN (); ++j)
        {
          if (i == j) continue;
          Ptr<Socket> srcSocket = Socket::CreateSocket (vehicles.Get (i), tid);
          Address destAddress = InetSocketAddress (interfaces.GetAddress (j), 5000);

          Ptr<BsmApp> app = CreateObject<BsmApp> ();
          app->Setup (srcSocket, destAddress, bsmPacketSize, uint32_t (simulationTime/interval), Seconds (interval));
          vehicles.Get (i)->AddApplication (app);
          app->SetStartTime (Seconds (1.0));
          app->SetStopTime (Seconds (simulationTime));
        }
    }

  AnimationInterface anim ("vanet-anim.xml");
  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get(i), "Vehicle"+std::to_string(i+1));
      anim.UpdateNodeColor (vehicles.Get(i), 0, 0, 255); // blue
    }
  anim.SetMaxPktsPerTraceFile (500000);

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}