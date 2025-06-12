#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"
#include "ns3/udp-client-server-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergyModelSimulation");

static void
RemainingEnergy (double remaining)
{
  NS_LOG_UNCOND ("Remaining energy = " << remaining << " J");
}

static void
TotalEnergyConsumed (double total)
{
  NS_LOG_UNCOND ("Total energy consumed by radio = " << total << " J");
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  Time simulationStartTime = Seconds (0.0);
  double nodeDistance = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("packetSize", "Size of the packets to send", packetSize);
  cmd.AddValue ("simulationStartTime", "Start time of the simulation", simulationStartTime);
  cmd.AddValue ("nodeDistance", "Distance between the two nodes", nodeDistance);
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
  Ipv4InterfaceContainer interfaces = address.Assign (apDevices);
  address.Assign (staDevices);

  UdpServerHelper server (9);
  ApplicationContainer apps = server.Install (nodes.Get (0));
  apps.Start (simulationStartTime);
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (0), 9);
  client.SetPacketSize (packetSize);
  client.SetAttribute ("MaxPackets", UintegerValue (4000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  apps = client.Install (nodes.Get (1));
  apps.Start (simulationStartTime + Seconds(1.0));
  apps.Stop (Seconds (10.0));

  /* Energy Model */
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (1));

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0194));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.0015));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.00009));
  radioEnergyModelHelper.Install (staDevices, sources);

  Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource> (sources.Get (0));
  basicSourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));

  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.0015));
  Ptr<WifiRadioEnergyModel> radioEnergyModelPtr = DynamicCast<WifiRadioEnergyModel> (basicSourcePtr->FindDeviceEnergyModel ("ns3::WifiRadioEnergyModel"));
  radioEnergyModelPtr->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergyConsumed));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}