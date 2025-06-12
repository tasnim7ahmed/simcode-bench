#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

class EnergyHarvestingApp : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    return TypeId ("EnergyHarvestingApp")
      .SetParent<Application> ()
      .AddConstructor<EnergyHarvestingApp> ();
  }

  EnergyHarvestingApp () : m_socket (0), m_peer (), m_packetSize (1024), m_sendEvent (), m_running (false) {}
  virtual ~EnergyHarvestingApp () { }

  void Setup (Ptr<Socket> socket, Address peer, uint32_t packetSize)
  {
    m_socket = socket;
    m_peer = peer;
    m_packetSize = packetSize;
  }

protected:
  virtual void StartApplication (void)
  {
    m_running = true;
    m_socket->Bind ();
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  virtual void StopApplication (void)
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

private:
  void SendPacket (void)
  {
    if (m_running && Simulator::Now ().GetSeconds () < 10.0)
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      m_socket->Send (packet);

      NS_LOG_UNCOND ("Sent packet at " << Simulator::Now ().GetSeconds () << "s");
      m_sendEvent = Simulator::Schedule (Seconds (1), &EnergyHarvestingApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  EventId m_sendEvent;
  bool m_running;
};

static void LogEnergyStats (Ptr<const EnergySource> energySrc, uint32_t nodeId)
{
  double totalEnergyConsumed = energySrc->GetTotalEnergyConsumed ();
  double residualEnergy = energySrc->GetRemainingEnergy ();
  double supplyVoltage = energySrc->GetSupplyVoltage ();

  NS_LOG_UNCOND ("Node " << nodeId << ": Total Energy Consumed = " << totalEnergyConsumed << " J, Residual Energy = " << residualEnergy << " J, Supply Voltage = " << supplyVoltage << " V");
}

static void UpdateHarvesterPower (Ptr<EnergyHarvester> harvester, Ptr<UniformRandomVariable> randomPower)
{
  double power = randomPower->GetValue (0.0, 0.1); // W
  harvester->SetHarvestedPower (Watts (power));
  NS_LOG_UNCOND ("Updated harvested power: " << power << " W at time " << Simulator::Now ().GetSeconds ());

  if (Simulator::Now ().GetSeconds () < 10.0)
  {
    Simulator::Schedule (Seconds (1), &UpdateHarvesterPower, harvester, randomPower);
  }
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("EnergyHarvestingApp", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (channel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer sinkApps = server.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  EnergySourceHelper batterySourceHelper;
  batterySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (1.0));
  batterySourceHelper.Set ("BasicEnergySourceSupplyVoltageV", DoubleValue (3.0));
  batterySourceHelper.Set ("BasicEnergySourceCurrentA", DoubleValue (0.0174)); // TX current

  EnergySourceContainer sources = batterySourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));

  radioEnergyHelper.Install (devices, sources);

  UniformRandomVariable randPower;
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    BasicEnergyHarvesterHelper harvesterHelper;
    harvesterHelper.Set ("PeriodicHarvestedPowerUpdateInterval", TimeValue (Seconds (1)));
    harvesterHelper.Set ("InitialHarvestedPower", WattsValue (Watts (randPower.GetValue (0.0, 0.1))));
    harvesterHelper.Install (nodes.Get (i), sources.Get (i));
  }

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback (&LogEnergyStats));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/Tx", MakeCallback (&LogEnergyStats));

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    Ptr<Node> node = nodes.Get (i);
    Ptr<UniformRandomVariable> randVar = CreateObject<UniformRandomVariable> ();
    randVar->SetAttribute ("Min", DoubleValue (0.0));
    randVar->SetAttribute ("Max", DoubleValue (0.1));
    Simulator::ScheduleNow (&UpdateHarvesterPower, node->GetObject<EnergyHarvester> (), randVar);
  }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
    LogEnergyStats (sources.Get (i), i);
  }

  Simulator::Destroy ();
  return 0;
}