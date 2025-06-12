#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/buildings-module.h"
#include "ns3/antenna-module.h"
#include "ns3/v2x-channel-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularBeamformingSimulation");

class VehicularScenario : public Object {
public:
  static TypeId GetTypeId (void);
  VehicularScenario ();
  virtual ~VehicularScenario ();

  void SetupSimulation ();
  void LogMeasurements ();
  void PrintAverageResults ();

private:
  NodeContainer m_vehicles;
  NetDeviceContainer m_devices;
  Ptr<PropagationLossModel> m_lossModel;
  std::ofstream m_snrLog;
  std::ofstream m_pathlossLog;
  double m_totalSnr;
  double m_totalPathloss;
  uint32_t m_sampleCount;
};

TypeId
VehicularScenario::GetTypeId (void)
{
  static TypeId tid = TypeId ("VehicularScenario")
    .SetParent<Object> ()
    .AddConstructor<VehicularScenario> ()
  ;
  return tid;
}

VehicularScenario::VehicularScenario ()
  : m_totalSnr (0.0),
    m_totalPathloss (0.0),
    m_sampleCount (0)
{
  m_snrLog.open ("snr-log.txt");
  m_pathlossLog.open ("pathloss-log.txt");
}

VehicularScenario::~VehicularScenario ()
{
  m_snrLog.close ();
  m_pathlossLog.close ();
}

void
VehicularScenario::SetupSimulation ()
{
  m_vehicles.Create (4);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (m_vehicles);

  for (NodeContainer::Iterator i = m_vehicles.Begin (); i != m_vehicles.End (); ++i)
    {
      Vector position = Vector (0, 0, 1.5); // Ground vehicles, z = 1.5m
      (*i)->GetObject<MobilityModel>()->SetPosition (position);
    }

  m_vehicles.Get (0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (20, 0, 0)); // 20 m/s
  m_vehicles.Get (1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (25, 0, 0));
  m_vehicles.Get (2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (-15, 0, 0));
  m_vehicles.Get (3)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (0, 0, 0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::ThreeGppBuildingsPropagationLossModel");

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (wifiChannel.Create ());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211bd);

  mac.SetType ("ns3::AdhocWifiMac");
  m_devices = wifi.Install (phy, mac, m_vehicles);

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings/DlBandwidth", UintegerValue (106));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings/UlBandwidth", UintegerValue (106));

  Ptr<UniformPlanarArray> antennaArray = CreateObject<UniformPlanarArray> ();
  antennaArray->SetNumColumns (4);
  antennaArray->SetNumRows (1);
  antennaArray->SetAntennaElement (CreateObject<IsotropicAntennaModel>());

  for (auto deviceIt = m_devices.Begin (); deviceIt != m_devices.End (); ++deviceIt)
    {
      Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (*deviceIt);
      dev->GetPhy ()->SetAttribute ("Antenna", PointerValue (antennaArray));
      dev->GetPhy ()->SetAttribute ("BeamformingMethod", StringValue ("ns3::DftBasedBeamforming"));
    }

  m_lossModel = wifiChannel.GetChannel ()->GetObject<PropagationLossModel> ();

  Simulator::Schedule (Seconds (0.1), &VehicularScenario::LogMeasurements, this);
  Simulator::Schedule (Seconds (10.0), &VehicularScenario::PrintAverageResults, this);
}

void
VehicularScenario::LogMeasurements ()
{
  for (uint32_t i = 0; i < m_vehicles.GetN (); ++i)
    {
      for (uint32_t j = i + 1; j < m_vehicles.GetN (); ++j)
        {
          Ptr<MobilityModel> a = m_vehicles.Get (i)->GetObject<MobilityModel> ();
          Ptr<MobilityModel> b = m_vehicles.Get (j)->GetObject<MobilityModel> ();

          double lossDb = m_lossModel->CalcRxPower (0, a, b);
          double snr = 10 - lossDb; // Simplified assumption

          m_snrLog << Simulator::Now ().GetSeconds () << " " << i << "-" << j << " " << snr << std::endl;
          m_pathlossLog << Simulator::Now ().GetSeconds () << " " << i << "-" << j << " " << lossDb << std::endl;

          m_totalSnr += snr;
          m_totalPathloss += lossDb;
          m_sampleCount++;
        }
    }

  if (Simulator::Now ().GetSeconds () < 10.0)
    {
      Simulator::Schedule (Seconds (0.1), &VehicularScenario::LogMeasurements, this);
    }
}

void
VehicularScenario::PrintAverageResults ()
{
  NS_LOG_UNCOND ("Average SNR: " << (m_totalSnr / m_sampleCount) << " dB");
  NS_LOG_UNCOND ("Average Pathloss: " << (m_totalPathloss / m_sampleCount) << " dB");
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  VehicularScenario scenario;
  scenario.SetupSimulation ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}