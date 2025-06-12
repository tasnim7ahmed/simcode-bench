#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

class EnergyLog : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    TypeId tid = Application::GetTypeId ();
    return tid;
  }

  EnergyLog () {}
  virtual ~EnergyLog () {}

  void SetNode (Ptr<Node> node)
  {
    m_node = node;
  }

protected:
  void DoStart (void) override
  {
    Simulator::Schedule (Seconds (0.0), &EnergyLog::Log, this);
  }

private:
  void Log (void)
  {
    Ptr<BasicEnergySource> energySrc = DynamicCast<BasicEnergySource> (m_node->GetObject<EnergySourceContainer> ()->Get (0));
    if (energySrc)
      {
        NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s | Residual Energy: " << energySrc->GetRemainingEnergy () << " J | Total Energy Consumed: " << energySrc->GetTotalEnergyConsumed () << " J");
      }
    Ptr<WifiRadioEnergyModel> wifiModel = DynamicCast<WifiRadioEnergyModel> (energySrc->FindDeviceEnergyModel ("ns3::WifiRadioEnergyModel"));
    if (wifiModel)
      {
        NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s | Wifi Energy Consumed: " << wifiModel->GetTotalEnergyConsumption () << " J");
      }
    if (Simulator::Now () < Seconds (10))
      {
        Simulator::Schedule (Seconds (1.0), &EnergyLog::Log, this);
      }
  }

  Ptr<Node> m_node;
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("EnergyEfficientWirelessSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", Seconds (1.0));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  // Energy Model Setup
  BasicEnergySourceHelper energySource;
  energySource.Set ("InitialEnergy", DoubleValue (1.0)); // 1 Joule initial energy
  energySource.Set ("SupplyVoltage", DoubleValue (3.0)); // Arbitrary voltage

  // Random Harvester
  Ptr<UniformRandomVariable> powerHarvest = CreateObject<UniformRandomVariable> ();
  powerHarvest->SetAttribute ("Min", DoubleValue (0.0));
  powerHarvest->SetAttribute ("Max", DoubleValue (0.1));

  class Harvester : public EnergyHarvester
  {
  public:
    static TypeId GetTypeId ()
    {
      return TypeId ("Harvester").SetParent<EnergyHarvester> ();
    }
    Harvester (Ptr<UniformRandomVariable> rv) : m_rv (rv), m_lastUpdate (Seconds (0)) {}
    double DoGetPower (void) override
    {
      if (Simulator::Now () - m_lastUpdate >= Seconds (1))
        {
          m_power = m_rv->GetValue ();
          m_lastUpdate = Simulator::Now ();
        }
      NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << "s | Harvested Power: " << m_power << " W");
      return m_power;
    }
  private:
    Ptr<UniformRandomVariable> m_rv;
    double m_power = 0.0;
    Time m_lastUpdate;
  };

  ObjectFactory harvesterFactory;
  harvesterFactory.SetTypeId ("Harvester");
  harvesterFactory.Set ("m_rv", PointerValue (powerHarvest));

  for (auto node : nodes)
    {
      EnergySourceContainer sources = energySource.Install (node);
      sources.Get (0)->AddEnergyHarvester (harvesterFactory.Create ());
    }

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));

  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, energySource.Find (nodes));

  // Logging application on receiver node
  Ptr<EnergyLog> logger = CreateObject<EnergyLog> ();
  logger->SetNode (nodes.Get (1));
  nodes.Get (1)->AddApplication (logger);
  logger->SetStartTime (Seconds (0.0));
  logger->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Print final stats
  for (size_t i = 0; i < 2; ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<BasicEnergySource> energySrc = DynamicCast<BasicEnergySource> (node->GetObject<EnergySourceContainer> ()->Get (0));
      if (energySrc)
        {
          std::cout << "Node " << i << ": Final Residual Energy = " << energySrc->GetRemainingEnergy () << " J" << std::endl;
          std::cout << "Node " << i << ": Total Energy Consumed = " << energySrc->GetTotalEnergyConsumed () << " J" << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}