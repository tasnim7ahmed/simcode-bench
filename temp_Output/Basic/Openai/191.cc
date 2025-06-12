#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnEnergySimulation");

class CustomApp : public Application
{
public:
  CustomApp () : m_socket (0), m_peer (), m_packetSize (50), m_interval (Seconds (2.0)), m_running (false), m_energyDepleted (false) {}

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_interval = interval;
  }

  void SetEnergySource (Ptr<EnergySource> source)
  {
    m_energySource = source;
    m_energySource->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&CustomApp::CheckEnergy, this));
  }

  bool IsEnergyDepleted () const
  {
    if (!m_energySource) return false;
    return m_energySource->GetRemainingEnergy () <= 0.0;
  }

private:
  virtual void StartApplication (void)
  {
    m_running = true;
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    ScheduleTx ();
  }

  virtual void StopApplication (void)
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel (m_sendEvent);
    if (m_socket)
      m_socket->Close ();
  }

  void ScheduleTx ()
  {
    if (m_running && (!IsEnergyDepleted()))
      m_sendEvent = Simulator::Schedule (m_interval, &CustomApp::SendPacket, this);
  }

  void SendPacket ()
  {
    if (!IsEnergyDepleted())
      {
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        m_socket->Send (packet);
        ScheduleTx ();
      }
  }

  void CheckEnergy (double oldValue, double newValue)
  {
    if (newValue <= 0.0 && !m_energyDepleted)
      {
        m_energyDepleted = true;
        StopApplication ();
      }
  }

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  Time            m_interval;
  EventId         m_sendEvent;
  bool            m_running;
  bool            m_energyDepleted;
  Ptr<EnergySource> m_energySource;
};

void CheckAllDead (std::vector< Ptr<CustomApp> > apps)
{
  bool anyAlive = false;
  for (auto app : apps)
    {
      if (app && !app->IsEnergyDepleted ())
        {
          anyAlive = true;
          break;
        }
    }
  if (!anyAlive)
    {
      Simulator::Stop ();
    }
  else
    {
      Simulator::Schedule (Seconds (1.0), &CheckAllDead, apps);
    }
}

int main (int argc, char *argv[])
{
  uint32_t gridWidth = 4;
  uint32_t gridHeight = 4;
  double nodeDistance = 10.0;
  double initialEnergy = 2.0; // Joules
  uint32_t packetSize = 50; // bytes
  double interval = 2.0; // seconds

  // Last node is sink
  uint32_t numSourceNodes = gridWidth * gridHeight;
  uint32_t numNodes = numSourceNodes + 1;

  CommandLine cmd;
  cmd.AddValue ("gridWidth", "Grid width", gridWidth);
  cmd.AddValue ("gridHeight", "Grid height", gridHeight);
  cmd.AddValue ("nodeDistance", "Distance between nodes", nodeDistance);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Mobility: grid for sensor nodes, centralized sink node
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (nodeDistance),
                                "DeltaY", DoubleValue (nodeDistance),
                                "GridWidth", UintegerValue (gridWidth),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (nodes.GetN () - 1); // install on all but last (sink)

  Ptr<ListPositionAllocator> sinkPosAlloc = CreateObject<ListPositionAllocator> ();
  double sinkX = nodeDistance * (gridWidth - 1) / 2.0;
  double sinkY = nodeDistance * gridHeight + nodeDistance;
  sinkPosAlloc->Add (Vector (sinkX, sinkY, 0.0));
  MobilityHelper sinkMob;
  sinkMob.SetPositionAllocator (sinkPosAlloc);
  sinkMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  sinkMob.Install (nodes.Get (numNodes - 1));

  // WiFi setup (ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Energy model on all source nodes (sink has unlimited energy)
  BasicEnergySourceHelper energyHelper;
  energyHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergy));
  EnergySourceContainer sources = energyHelper.Install (nodes);
  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);

  // Stack & routing
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // The sink is the last node
  Address sinkAddress = InetSocketAddress (interfaces.GetAddress (numNodes - 1), 9000);

  // Sink application
  uint16_t sinkPort = 9000;
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (numNodes - 1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10000.0));

  // Setup source applications and collect pointers
  std::vector< Ptr<CustomApp> > appPtrs;

  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      Ptr<Socket> srcSocket = Socket::CreateSocket (nodes.Get (i), UdpSocketFactory::GetTypeId ());
      Ptr<CustomApp> app = CreateObject<CustomApp> ();
      app->Setup (srcSocket, sinkAddress, packetSize, Seconds (interval));
      app->SetEnergySource (sources.Get (i));
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0));
      app->SetStopTime (Seconds (10000.0));
      appPtrs.push_back (app);
    }

  // Schedule energy monitoring
  Simulator::Schedule (Seconds (1.0), &CheckAllDead, appPtrs);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}