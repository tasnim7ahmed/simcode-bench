#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergyModel");

static void RemainingEnergy (double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND ("Remaining Energy = " << remainingEnergy << " J");
}

static void TotalEnergyConsumed (double oldValue, double totalEnergyConsumed)
{
  NS_LOG_UNCOND ("Total Energy Consumed = " << totalEnergyConsumed << " J");
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  Time startTime = Seconds (0.0);
  double nodeDistance = 1.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("startTime", "Start Time", startTime);
  cmd.AddValue ("nodeDistance", "Distance between nodes", nodeDistance);
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

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

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign (staDevices);
  interfaces = address.Assign (apDevices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (0));
  apps.Start (startTime);
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (0), 9);
  client.SetPacketSize (packetSize);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  apps = client.Install (nodes.Get (1));
  apps.Start (startTime + Seconds(0.1));
  apps.Stop (Seconds (10.0));

  EnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("NominalVoltage", DoubleValue (3.7));

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("PeriodicVacancyCheck", BooleanValue (true));
  basicSourceHelper.Set ("VacancyCheckPeriod", TimeValue (Seconds (1.0)));

  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (0));
  EnergySourceContainer sources1 = basicSourceHelper.Install (nodes.Get (1));

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.0011));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.000090));
  radioEnergyModelHelper.Install (staDevices, sources1);
  radioEnergyModelHelper.Install (apDevices, sources);

  Ptr<EnergySource> sourcePtr = sources.Get (0);
  sourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  sourcePtr->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));

  Ptr<EnergySource> sourcePtr1 = sources1.Get (0);
  sourcePtr1->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  sourcePtr1->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  double finalEnergyNode0 = sourcePtr->GetRemainingEnergy();
  double finalEnergyNode1 = sourcePtr1->GetRemainingEnergy();
  NS_ASSERT (finalEnergyNode0 > 0 && finalEnergyNode1 > 0);

  return 0;
}