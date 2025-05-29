#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnEnergySimulation");

// Custom application to generate traffic and detect energy depletion
class SensorApp : public Application
{
public:
  SensorApp ();
  virtual ~SensorApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, Ptr<EnergySource> energy);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  uint32_t        m_packetsSent;
  Time            m_pktInterval;
  EventId         m_sendEvent;
  bool            m_running;
  Ptr<EnergySource> m_energy;

  void EnergyDepletedCallback ();
};

SensorApp::SensorApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_packetsSent (0),
    m_pktInterval (Seconds (1.0)),
    m_sendEvent (),
    m_running (false),
    m_energy (nullptr)
{
}

SensorApp::~SensorApp ()
{
  m_socket = 0;
}

void
SensorApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, Ptr<EnergySource> energy)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
  m_energy = energy;
}

void
SensorApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);

  if (m_energy)
    {
      m_energy->TraceConnectWithoutContext ("EnergyDepleted", MakeCallback (&SensorApp::EnergyDepletedCallback, this));
    }

  SendPacket ();
}

void
SensorApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void
SensorApp::SendPacket (void)
{
  if (!m_running)
    return;

  if (m_energy && m_energy->GetRemainingEnergy () <= 0)
    {
      StopApplication ();
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
  if (m_energy && m_energy->GetRemainingEnergy () <= 0)
    {
      StopApplication ();
      return;
    }

  m_sendEvent = Simulator::Schedule (m_pktInterval, &SensorApp::SendPacket, this);
}

void
SensorApp::EnergyDepletedCallback ()
{
  StopApplication ();
}

static bool
AllNodesDepleted (const NodeContainer& nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<EnergySourceContainer> esc = node->GetObject<EnergySourceContainer> ();
      if (esc && esc->GetN () > 0)
        {
          Ptr<EnergySource> src = esc->Get (0);
          if (src->GetRemainingEnergy () > 0)
            return false;
        }
    }
  return true;
}

void
CheckEnergy (NodeContainer sensors)
{
  if (AllNodesDepleted (sensors))
    {
      NS_LOG_UNCOND ("All sensor nodes depleted energy at: " << Simulator::Now ().GetSeconds () << "s. Stopping simulation.");
      Simulator::Stop ();
    }
  else
    {
      Simulator::Schedule (Seconds (1.0), &CheckEnergy, sensors);
    }
}

int
main (int argc, char *argv[])
{
  uint32_t gridWidth = 5;
  uint32_t gridHeight = 5;
  double gridStep = 20.0;
  uint32_t nSensors = gridWidth * gridHeight;
  uint32_t packetSize = 50;
  uint32_t totalPackets = 10000;
  double interval = 2.0;
  double initialEnergy = 0.2; // Joules per sensor, must be small; packet cost will drain this
  double txCurrent = 0.017; // ~17mA for MICA2 (typical sensor node)
  double rxCurrent = 0.015; // ~15mA for MICA2
  double supplyVoltage = 3.0; // Volts

  CommandLine cmd;
  cmd.AddValue ("gridWidth", "Number of sensors horizontally", gridWidth);
  cmd.AddValue ("gridHeight", "Number of sensors vertically", gridHeight);
  cmd.AddValue ("interval", "Packet send interval", interval);
  cmd.AddValue ("initialEnergy", "Initial energy (Joules)", initialEnergy);
  cmd.Parse (argc, argv);

  NodeContainer sensorNodes;
  sensorNodes.Create (nSensors);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (sensorNodes);
  allNodes.Add (sinkNode);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;

  Ssid ssid = Ssid ("wsn-ssid");

  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // Set mobility in grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (gridStep),
                                "DeltaY", DoubleValue (gridStep),
                                "GridWidth", UintegerValue (gridWidth),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  // Sink as central in the grid
  Ptr<ListPositionAllocator> sinkPos = CreateObject<ListPositionAllocator> ();
  double cx = (gridWidth-1)*gridStep/2.0;
  double cy = (gridHeight-1)*gridStep/2.0;
  sinkPos->Add (Vector (cx, cy, 0.0));
  mobility.SetPositionAllocator (sinkPos);
  mobility.Install (sinkNode);

  // Energy source and model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergy));
  energySourceHelper.Set ("BasicEnergySupplyVoltageV", DoubleValue (supplyVoltage));

  EnergySourceContainer sources = energySourceHelper.Install (sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (txCurrent));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (rxCurrent));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices.GetN () - 1, devices.GetN () - 1, sources);

  // Internet stack & routing
  InternetStackHelper internet;
  Ipv4ListRoutingHelper list;
  Ipv4StaticRoutingHelper staticRh;
  list.Add (staticRh, 1);
  internet.SetRoutingHelper (list);
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Applications:
  // Sink: UDP server
  uint16_t sinkPort = 4000;
  Address sinkAddress (InetSocketAddress (interfaces.Get (nSensors), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (sinkNode);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10000.0));

  // Sensor nodes: custom sensor app for all except sink
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      Ptr<Socket> srcSocket = Socket::CreateSocket (sensorNodes.Get (i), UdpSocketFactory::GetTypeId ());
      Ptr<EnergySourceContainer> esc = sensorNodes.Get (i)->GetObject<EnergySourceContainer> ();
      Ptr<EnergySource> srcEnergy = nullptr;
      if (esc && esc->GetN () > 0)
        srcEnergy = esc->Get (0);

      Ptr<SensorApp> app = CreateObject<SensorApp> ();
      app->Setup (srcSocket, sinkAddress, packetSize, totalPackets, Seconds (interval), srcEnergy);
      sensorNodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i * 0.1));
      app->SetStopTime (Seconds (10000.0));
    }

  // Enable logging/tracing
  // Uncomment to enable
  //wifiPhy.EnablePcapAll ("wsn-energy");

  Simulator::Schedule (Seconds (1.0), &CheckEnergy, sensorNodes);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}