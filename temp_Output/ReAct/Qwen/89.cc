#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
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
  void DoInitialize (void) override
  {
    Simulator::Schedule (Seconds (0.0), &EnergyLog::Log, this);
  }

private:
  void Log (void)
  {
    Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource> (m_node->GetObject<EnergySourceContainer>()->Get (0));
    NS_ASSERT_MSG (energySource, "No BasicEnergySource found for node");
    double residualEnergy = energySource->GetRemainingEnergy ();

    std::cout << Simulator::Now ().GetSeconds () << "s Residual Energy: " << residualEnergy << "J" << std::endl;

    if (Simulator::Now () < Seconds (10))
      Simulator::Schedule (Seconds (1.0), &EnergyLog::Log, this);
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

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (channel.Create ());

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

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
  onoff.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (nodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  EnergySourceContainer sources;
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJ", DoubleValue (1.0));
  sources.Add (basicSourceHelper.Install (nodes));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);

  BasicEnergyHarvesterHelper harvesterHelper;
  Ptr<UniformRandomVariable> powerRv = CreateObject<UniformRandomVariable>();
  powerRv->SetAttribute ("Min", DoubleValue (0.0));
  powerRv->SetAttribute ("Max", DoubleValue (0.1));
  harvesterHelper.Set ("HarvestedPower", RandomVariableValue (powerRv));
  harvesterHelper.Set ("UpdateInterval", TimeValue (Seconds (1.0)));

  EnergyHarvesterContainer harvesters = harvesterHelper.Install (nodes, sources);

  EnergyLog logApp;
  logApp.SetNode (nodes.Get (1));
  nodes.Get (1)->AddApplication (&logApp);
  logApp.SetStartTime (Seconds (0.0));
  logApp.SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  for (uint32_t i = 0; i < sources.GetN (); ++i)
  {
    Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource> (sources.Get (i));
    NS_ASSERT (src != nullptr);
    double totalEnergyConsumed = src->GetTotalEnergyConsumed ();
    double remainingEnergy = src->GetRemainingEnergy ();
    double harvestedEnergy = src->GetHarvestedEnergy ();
    std::cout << "Node " << i << ": Total Consumed Energy = " << totalEnergyConsumed << "J, Remaining Energy = " << remainingEnergy << "J, Harvested Energy = " << harvestedEnergy << "J" << std::endl;
  }

  Simulator::Destroy ();
  return 0;
}