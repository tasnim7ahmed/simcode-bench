#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocEnergyDepletionExample");

void
RemainingEnergyTrace (Ptr<OutputStreamWrapper> stream, double oldValue, double newValue)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "s" << "\t" << newValue << " J" << std::endl;
}

void
DeviceStateChangeCallback (int64_t nodeId, const WifiPhyStateHelper::State newState)
{
  if (newState == WifiPhyStateHelper::State::aSLEEP)
    {
      NS_LOG_UNCOND ("Node " << nodeId << " entered SLEEP state at " << Simulator::Now ().GetSeconds () << "s");
    }
}

void
EnergyDepletionCallback (Ptr<Node> node)
{
  NS_LOG_UNCOND ("Node " << node->GetId () << " energy depleted at " << Simulator::Now ().GetSeconds () << "s");
}

int
main (int argc, char *argv[])
{
  double simulationTime = 20.0;
  std::string dataRate = "1Mbps";
  uint32_t packetSize = 512;
  double txPowerStart = 16.0;
  double txPowerEnd = 16.0;
  double initialEnergy = 0.3;
  double txCurrentA = 0.0174;
  double rxCurrentA = 0.0197;
  double idleCurrentA = 0.013;
  double sleepCurrentA = 0.000045;

  CommandLine cmd;
  cmd.AddValue ("dataRate", "OnOffApplication Data Rate", dataRate);
  cmd.AddValue ("packetSize", "OnOffApplication Packet Size (bytes)", packetSize);
  cmd.AddValue ("txPowerStart", "WiFi Tx Power Start (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "WiFi Tx Power End (dBm)", txPowerEnd);
  cmd.AddValue ("initialEnergy", "Initial Energy (J)", initialEnergy);
  cmd.AddValue ("txCurrentA", "Tx Current (A)", txCurrentA);
  cmd.AddValue ("rxCurrentA", "Rx Current (A)", rxCurrentA);
  cmd.AddValue ("idleCurrentA", "Idle Current (A)", idleCurrentA);
  cmd.AddValue ("sleepCurrentA", "Sleep Current (A)", sleepCurrentA);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-96.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-99.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (30.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergy));

  EnergySourceContainer energySources = energySourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (txCurrentA));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (rxCurrentA));
  radioEnergyHelper.Set ("IdleCurrentA", DoubleValue (idleCurrentA));
  radioEnergyHelper.Set ("SleepCurrentA", DoubleValue (sleepCurrentA));

  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, energySources);

  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  // Set up trace output files
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> energyStream = ascii.CreateFileStream ("energy-trace.log");
  Ptr<OutputStreamWrapper> stateStream = ascii.CreateFileStream ("state-transition.log");

  Ptr<BasicEnergySource> energySource0 = DynamicCast<BasicEnergySource> (energySources.Get (0));
  Ptr<WifiRadioEnergyModel> wifiRadio0 = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (0));
  Ptr<BasicEnergySource> energySource1 = DynamicCast<BasicEnergySource> (energySources.Get (1));
  Ptr<WifiRadioEnergyModel> wifiRadio1 = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (1));

  // Trace remaining energy for both nodes
  energySource0->TraceConnectWithoutContext ("RemainingEnergy", MakeBoundCallback (&RemainingEnergyTrace, energyStream));
  energySource1->TraceConnectWithoutContext ("RemainingEnergy", MakeBoundCallback (&RemainingEnergyTrace, energyStream));

  // Connect depletion callback
  energySource0->TraceConnectWithoutContext ("EnergyDepleted", MakeCallback (&EnergyDepletionCallback));
  energySource1->TraceConnectWithoutContext ("EnergyDepletion", MakeCallback (&EnergyDepletionCallback));

  // Connect to the WifiPhy state change
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
      Ptr<WifiPhy> phy = wifiDev->GetPhy ();

      phy->TraceConnect ("State", std::to_string (i),
        MakeBoundCallback ([nodeId = int64_t(i), stateStream] (const std::string &,
                               WifiPhyStateHelper::State oldState, WifiPhyStateHelper::State newState) {
          *stateStream->GetStream () << Simulator::Now ().GetSeconds () << "s\t"
                                     << "Node " << nodeId << ": " << oldState << "->" << newState << std::endl;
          DeviceStateChangeCallback (nodeId, newState);
        }));
    }

  Simulator::Stop (Seconds (simulationTime + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}