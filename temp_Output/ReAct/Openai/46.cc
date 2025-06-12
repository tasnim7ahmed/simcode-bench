#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocEnergyDepletionExample");

void
RemainingEnergyTrace(double oldValue, double newValue)
{
  static std::ofstream energyLog("energy-trace.log", std::ios_base::app);
  energyLog << Simulator::Now ().GetSeconds () << "\t" << newValue << std::endl;
}

void
DeviceStateChangedTrace(std::string context, DeviceEnergyModel::DeviceState oldState, DeviceEnergyModel::DeviceState newState)
{
  static std::ofstream stateLog("state-trace.log", std::ios_base::app);
  stateLog << Simulator::Now ().GetSeconds () << "\t" << oldState << "\t" << newState << std::endl;
  if (newState == DeviceEnergyModel::DeviceState::DEVICE_STATE_SLEEP)
    {
      NS_LOG_UNCOND("Node entered sleep state at " << Simulator::Now ().GetSeconds () << " seconds");
    }
}

void
NodeEntersSleep(uint32_t nodeId)
{
  NS_LOG_UNCOND("Node " << nodeId << " entered sleep state due to energy depletion at " << Simulator::Now ().GetSeconds () << " seconds");
}

int
main (int argc, char *argv[])
{
  double simulationTime = 20.0; // seconds
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 512; // bytes
  double txPowerStart = 16.0; // dBm
  double txPowerEnd = 16.0; // dBm
  double initialEnergy = 5.0; // Joules
  double supplyVoltage = 3.7; // Volts
  double idleCurrent = 0.273; // Amps
  double txCurrent = 0.380; // Amps 
  double rxCurrent = 0.313; // Amps
  double sleepCurrent = 0.033; // Amps

  CommandLine cmd (__FILE__);
  cmd.AddValue ("dataRate", "OnOff application data rate", dataRate);
  cmd.AddValue ("packetSize", "OnOff application packet size [bytes]", packetSize);
  cmd.AddValue ("txPowerStart", "Wifi Tx Power start [dBm]", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Wifi Tx Power end [dBm]", txPowerEnd);
  cmd.AddValue ("initialEnergy", "Initial energy of node [J]", initialEnergy);
  cmd.AddValue ("supplyVoltage", "EnergySource supply voltage [V]", supplyVoltage);
  cmd.AddValue ("idleCurrent", "Idle current of WifiPhy [A]", idleCurrent);
  cmd.AddValue ("txCurrent", "Tx current of WifiPhy [A]", txCurrent);
  cmd.AddValue ("rxCurrent", "Rx current of WifiPhy [A]", rxCurrent);
  cmd.AddValue ("sleepCurrent", "Sleep current of WifiPhy [A]", sleepCurrent);
  cmd.Parse (argc, argv);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Set wifi PHY and MAC
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-96.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-99.0));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Attach mobility (static positions)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  Ptr<MobilityModel> mob0 = nodes.Get (0)->GetObject<MobilityModel> ();
  Ptr<MobilityModel> mob1 = nodes.Get (1)->GetObject<MobilityModel> ();
  mob0->SetPosition (Vector (0, 0, 0));
  mob1->SetPosition (Vector (5, 0, 0));

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Energy model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergy));
  energySourceHelper.Set ("SupplyVoltage", DoubleValue (supplyVoltage));
  EnergySourceContainer sources = energySourceHelper.Install (nodes.Get (0));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (txCurrent));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (rxCurrent));
  radioEnergyHelper.Set ("IdleCurrentA", DoubleValue (idleCurrent));
  radioEnergyHelper.Set ("SleepCurrentA", DoubleValue (sleepCurrent));
  radioEnergyHelper.Set ("CcaBusyCurrentA", DoubleValue (rxCurrent));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices.Get (0), sources);

  // Connect trace sources
  Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource> (sources.Get (0));
  src->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergyTrace));

  Ptr<DeviceEnergyModel> devModel = deviceModels.Get (0);
  devModel->TraceConnect ("/DeviceEnergyModel/DeviceState", std::string (), MakeCallback (&DeviceStateChangedTrace));

  devModel->SetEnergyDepletionCallback (MakeBoundCallback (&NodeEntersSleep, 0));

  // Applications: OnOff traffic from node 0 to node 1
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  // Sink
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  // Enable verbose logging if desired
  // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

  // Schedule: (energy and state logging files are handled in trace callbacks)

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}