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

class EnergyLogger : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    return TypeId ("EnergyLogger")
      .SetParent<Application> ()
      .AddConstructor<EnergyLogger> ();
  }

  EnergyLogger () {}
  virtual ~EnergyLogger () {}

  void SetNode (Ptr<Node> node)
  {
    m_node = node;
  }

protected:
  void DoInitialize (void) override
  {
    Application::DoInitialize ();
    Simulator::Schedule (Seconds (0.5), &EnergyLogger::LogEnergy, this);
  }

private:
  void LogEnergy (void)
  {
    Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource> (m_node->GetObject<EnergySourceContainer> ()->Get (0));
    if (energySource)
    {
      double residual = energySource->GetRemainingEnergy ();
      double totalConsumed = energySource->GetEnergyConsumption ();
      NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Residual Energy: " << residual << " J | Total Consumed: " << totalConsumed << " J");
    }

    if (Simulator::Now () < Seconds (10))
      Simulator::Schedule (Seconds (1), &EnergyLogger::LogEnergy, this);
  }

  void StartApplication (void) {}
  void StopApplication (void) {}

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

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1kbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (128));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (nodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("InitialEnergyJ", DoubleValue (1.0));
  energySourceHelper.Set ("SupplyVoltageV", DoubleValue (3.0));

  for (auto i = 0; i < 2; ++i)
  {
    energySourceHelper.Install (nodes.Get (i));
  }

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, nodes.Get (0)->GetObject<EnergySource> ());
  deviceModels = radioEnergyHelper.Install (devices, nodes.Get (1)->GetObject<EnergySource> ());

  class RandomHarvesting : public Object
  {
  public:
    static TypeId GetTypeId (void)
    {
      return TypeId ("RandomHarvesting")
        .SetParent<Object> ()
        .AddConstructor<RandomHarvesting> ();
    }

    void Install (Ptr<Node> node)
    {
      m_energySource = node->GetObject<BasicEnergySource> ();
      ScheduleNext ();
    }

    void Harvest ()
    {
      double power = (double) rand () / RAND_MAX * 0.1;
      double duration = 1.0;
      double energy = power * duration;
      m_energySource->UpdateEnergy (energy);
      ScheduleNext ();
    }

    void ScheduleNext ()
    {
      Simulator::Schedule (Seconds (1), &RandomHarvesting::Harvest, this);
    }

  private:
    Ptr<BasicEnergySource> m_energySource;
  };

  ObjectFactory harvestingFactory;
  harvestingFactory.SetTypeId ("RandomHarvesting");
  Ptr<RandomHarvesting> harvester0 = harvestingFactory.Create<RandomHarvesting> ();
  harvester0->Install (nodes.Get (0));
  Ptr<RandomHarvesting> harvester1 = harvestingFactory.Create<RandomHarvesting> ();
  harvester1->Install (nodes.Get (1));

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("1024"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));

  Ptr<EnergyLogger> logger = CreateObject<EnergyLogger> ();
  logger->SetNode (nodes.Get (1));
  nodes.Get (1)->AddApplication (logger);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}