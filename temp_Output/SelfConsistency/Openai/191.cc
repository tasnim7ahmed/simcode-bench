#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnEnergySimulation");

class SensorApp : public Application
{
public:
  SensorApp ();
  virtual ~SensorApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, Ptr<BasicEnergySource> energySource);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);
  void CheckEnergy (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  EventId         m_sendEvent;
  uint32_t        m_packetsSent;
  Ptr<BasicEnergySource> m_energySource;
};

SensorApp::SensorApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_pktInterval (Seconds (1.0)),
    m_sendEvent (),
    m_packetsSent (0),
    m_energySource (0)
{
}

SensorApp::~SensorApp ()
{
  m_socket = 0;
}

void
SensorApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, Ptr<BasicEnergySource> energySource)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
  m_energySource = energySource;
}

void
SensorApp::StartApplication (void)
{
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
SensorApp::StopApplication (void)
{
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
SensorApp::SendPacket (void)
{
  // If out of energy, do not send.
  if (m_energySource->GetRemainingEnergy () <= 0.0)
    {
      NS_LOG_INFO ("Node " << GetNode ()->GetId () << " depleted energy. Stopping app.");
      return;
    }
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  ++m_packetsSent;

  if (m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
SensorApp::ScheduleTx (void)
{
  m_sendEvent = Simulator::Schedule (m_pktInterval, &SensorApp::SendPacket, this);
}

void
SensorApp::CheckEnergy (void)
{
  if (m_energySource->GetRemainingEnergy () <= 0.0)
    {
      if (m_sendEvent.IsRunning ())
        {
          Simulator::Cancel (m_sendEvent);
        }
      if (m_socket)
        {
          m_socket->Close ();
        }
    }
}


static bool
CheckAllNodesDepleted (std::vector<Ptr<BasicEnergySource>> &energySources)
{
  for (auto &src : energySources)
    {
      if (src->GetRemainingEnergy () > 0.0)
        {
          return false;
        }
    }
  return true;
}

void
StopSimulationIfDepleted (std::vector<Ptr<BasicEnergySource>> energySources)
{
  if (CheckAllNodesDepleted (energySources))
    {
      NS_LOG_UNCOND ("All nodes have depleted their energy. Stopping simulation at " << Simulator::Now ().GetSeconds () << "s.");
      Simulator::Stop ();
    }
  else
    {
      // Check again after 0.5s
      Simulator::Schedule (Seconds (0.5), &StopSimulationIfDepleted, energySources);
    }
}

int
main (int argc, char *argv[])
{
  uint32_t gridWidth = 4;
  uint32_t gridHeight = 4;
  double distance = 30.0;
  uint32_t numNodes = gridWidth * gridHeight;
  double initialEnergy = 0.1; // Joules, very low to deplete quickly for demo
  uint32_t packetSize = 32; // bytes
  uint32_t packetsPerNode = 500;
  double pktInterval = 1.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("gridWidth", "Number of grid columns", gridWidth);
  cmd.AddValue ("gridHeight", "Number of grid rows", gridHeight);
  cmd.AddValue ("distance", "Distance between nodes (m)", distance);
  cmd.AddValue ("initialEnergy", "Initial energy for each node (J)", initialEnergy);
  cmd.AddValue ("packetSize", "Packet size (bytes)", packetSize);
  cmd.AddValue ("packetsPerNode", "Number of packets per node", packetsPerNode);
  cmd.AddValue ("pktInterval", "Interval between packets (s)", pktInterval);
  cmd.Parse (argc, argv);

  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);
  NodeContainer sinkNode;
  sinkNode.Create (1);

  // Assign mobility (grid for sensors, sink at center)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (gridWidth),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  // Sink at center
  MobilityHelper sinkMobility;
  sinkMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  sinkMobility.Install (sinkNode);
  Ptr<MobilityModel> sinkMob = sinkNode.Get (0)->GetObject<MobilityModel> ();
  double centerX = 0.5 * distance * (gridWidth - 1);
  double centerY = 0.5 * distance * (gridHeight - 1);
  sinkMob->SetPosition (Vector (centerX, centerY, 0.0));

  // Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer sensorDevices = wifi.Install (wifiPhy, wifiMac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install (wifiPhy, wifiMac, sinkNode);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (sensorNodes);
  internet.Install (sinkNode);

  // Addressing
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorIfs = ipv4.Assign (sensorDevices);
  Ipv4InterfaceContainer sinkIf = ipv4.Assign (sinkDevice);

  // Energy Model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergy));
  // Energy model per node
  EnergySourceContainer energySources = energySourceHelper.Install (sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer radioModels = radioEnergyHelper.Install (sensorDevices, energySources);

  // Keep handles to energy sources for all nodes for stop-checking
  std::vector<Ptr<BasicEnergySource>> energySourcePtrs;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource> (energySources.Get (i));
      energySourcePtrs.push_back (src);
    }

  // Applications: one OnOffApplication at sink to receive, one custom sensor app per sensor node

  // Sink: receive on UDP port
  uint16_t sinkPort = 8888;
  Address sinkAddress (InetSocketAddress (sinkIf.GetAddress (0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (sinkNode);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10000.0));

  // Sensor nodes: custom SensorApp over UDP sockets
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      Ptr<Node> node = sensorNodes.Get (i);
      Ptr<Socket> srcSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
      Ptr<SensorApp> app = CreateObject<SensorApp> ();
      app->Setup (srcSocket, sinkAddress, packetSize, packetsPerNode, Seconds (pktInterval), 
                  DynamicCast<BasicEnergySource> (energySources.Get (i)));
      node->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i*0.001)); // slight staggering
      app->SetStopTime (Seconds (10000.0));
    }

  // Schedule periodic network depletion checks
  Simulator::Schedule (Seconds (0.5), &StopSimulationIfDepleted, energySourcePtrs);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}