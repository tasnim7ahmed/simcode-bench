#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergySimulation");

double node0EnergyConsumed = 0.0;
double node1EnergyConsumed = 0.0;
double node0RemainingEnergy = 0.0;
double node1RemainingEnergy = 0.0;

void
RemainingEnergyTrace0 (double oldValue, double remainingEnergy)
{
  node0RemainingEnergy = remainingEnergy;
}

void
RemainingEnergyTrace1 (double oldValue, double remainingEnergy)
{
  node1RemainingEnergy = remainingEnergy;
}

void
TotalEnergyConsumptionTrace0 (double oldValue, double totalEnergy)
{
  node0EnergyConsumed = totalEnergy;
}

void
TotalEnergyConsumptionTrace1 (double oldValue, double totalEnergy)
{
  node1EnergyConsumed = totalEnergy;
}

void
UdpRxLogger (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND ("Packet received: " << packet->GetSize () << " bytes");
}

int
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  double simStartTime = 1.0;
  double nodeDistance = 50.0;
  uint32_t nPackets = 10;
  double interval = 1.0; // in seconds
  double energyUpperLimit = 0.15; // Joules, upper limit for total energy consumption

  CommandLine cmd;
  cmd.AddValue ("packetSize", "UDP packet size in bytes", packetSize);
  cmd.AddValue ("simStartTime", "Simulation start time (s)", simStartTime);
  cmd.AddValue ("nodeDistance", "Distance between the two nodes (m)", nodeDistance);
  cmd.AddValue ("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue ("interval", "Interval between each packet (s)", interval);
  cmd.AddValue ("energyUpperLimit", "Energy consumption upper limit (Joules)", energyUpperLimit);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (0));
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (1));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (nodeDistance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (staDevices, apDevices));

  // Energy Source & WiFi Radio Energy Model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (0.2));
  energySourceHelper.Set ("BasicEnergySupplyVoltageV", DoubleValue (3.3));
  EnergySourceContainer sources = energySourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (staDevices, sources.Get (0));
  DeviceEnergyModelContainer deviceModelsAp = radioEnergyHelper.Install (apDevices, sources.Get (1));

  // Energy Trace Connections
  Ptr<BasicEnergySource> source0 = DynamicCast<BasicEnergySource> (sources.Get (0));
  Ptr<BasicEnergySource> source1 = DynamicCast<BasicEnergySource> (sources.Get (1));
  source0->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergyTrace0));
  source0->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergyConsumptionTrace0));
  source1->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergyTrace1));
  source1->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergyConsumptionTrace1));

  // Application layer: UDP client/server
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (simStartTime));
  serverApp.Stop (Seconds (simStartTime + nPackets * interval + 2));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (simStartTime + 1));
  clientApp.Stop (Seconds (simStartTime + nPackets * interval + 1));

  // Log UDP packets received at the server
  serverApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&UdpRxLogger));

  Simulator::Stop (Seconds (simStartTime + nPackets * interval + 3));
  Simulator::Run ();

  // Energy reporting
  NS_LOG_UNCOND ("Node 0: Energy Consumed: " << node0EnergyConsumed << " J, Remaining: " << node0RemainingEnergy << " J");
  NS_LOG_UNCOND ("Node 1: Energy Consumed: " << node1EnergyConsumed << " J, Remaining: " << node1RemainingEnergy << " J");
  if (node0EnergyConsumed > energyUpperLimit || node1EnergyConsumed > energyUpperLimit)
    {
      NS_LOG_UNCOND ("ERROR: Energy consumption exceeded the upper limit (" << energyUpperLimit << " J)!");
    }
  else
    {
      NS_LOG_UNCOND ("Energy consumption within specified limits.");
    }

  Simulator::Destroy ();
  return 0;
}