#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergyModel");

double totalEnergyConsumedNode0 = 0;
double remainingEnergyNode0 = 0;
double totalEnergyConsumedNode1 = 0;
double remainingEnergyNode1 = 0;

void RemainingEnergy (double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND ("Remaining Energy = " << remainingEnergy << " Joules");
}

void TotalEnergyConsumed (double oldValue, double totalEnergyConsumed)
{
  NS_LOG_UNCOND ("Total Energy Consumed = " << totalEnergyConsumed << " Joules");
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  Time simulationStartTime = Seconds (0.0);
  double nodeDistance = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("simulationStartTime", "Simulation start time (seconds)", simulationStartTime);
  cmd.AddValue ("nodeDistance", "Distance between nodes (meters)", nodeDistance);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (nodeDistance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign (staDevices);
  interfaces = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientServerHelper echoClient (interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (simulationStartTime + Seconds(1.0));
  clientApps.Stop (Seconds (10.0));

  UdpClientServerHelper echoServer (interfaces.GetAddress (0), port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (simulationStartTime);
  serverApps.Stop (Seconds (10.0));

  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);
  BasicEnergySource* basicSourcePtr = dynamic_cast<BasicEnergySource*> (sources.Get (0));
  basicSourcePtr->SetInitialEnergy (100.0);
  basicSourcePtr = dynamic_cast<BasicEnergySource*> (sources.Get (1));
  basicSourcePtr->SetInitialEnergy (100.0);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0194));
  EnergyModelContainer deviceModels = radioEnergyModelHelper.Install (staDevices.Get (0), sources.Get (1));
  deviceModels = radioEnergyModelHelper.Install (apDevices.Get (0), sources.Get (0));

  Ptr<EnergySource> source = sources.Get (0);
  source->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  source->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));
  source = sources.Get (1);
  source->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  source->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource> (sources.Get (0));
  totalEnergyConsumedNode0 = basicSource->GetTotalEnergyConsumption ();
  remainingEnergyNode0 = basicSource->GetRemainingEnergy ();
  NS_LOG_UNCOND ("Node 0: Total energy consumed = " << totalEnergyConsumedNode0 << " J");
  NS_LOG_UNCOND ("Node 0: Remaining energy = " << remainingEnergyNode0 << " J");

  basicSource = DynamicCast<BasicEnergySource> (sources.Get (1));
  totalEnergyConsumedNode1 = basicSource->GetTotalEnergyConsumption ();
  remainingEnergyNode1 = basicSource->GetRemainingEnergy ();
    NS_LOG_UNCOND ("Node 1: Total energy consumed = " << totalEnergyConsumedNode1 << " J");
    NS_LOG_UNCOND ("Node 1: Remaining energy = " << remainingEnergyNode1 << " J");

  Simulator::Destroy ();

  return 0;
}