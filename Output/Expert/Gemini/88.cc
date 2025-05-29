#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/packet.h"

using namespace ns3;

static void
RemainingEnergy (double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND ("Remaining energy = " << remainingEnergy << " J");
}

static void
TotalEnergyConsumed (double oldValue, double totalEnergyConsumed)
{
  NS_LOG_UNCOND ("Total energy consumed by radio = " << totalEnergyConsumed << " J");
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  Time simulationStartTime = Seconds (0.1);
  double nodeDistance = 10.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("startTime", "Simulation start time", simulationStartTime);
  cmd.AddValue ("nodeDistance", "Distance between nodes", nodeDistance);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (staDevices);
  interfaces = address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (simulationStartTime);
  clientApps.Stop (Seconds (10.0));

  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (0));
  EnergySourceContainer sources1 = basicSourceHelper.Install (nodes.Get (1));

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.01));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.005));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.001));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.0001));
  radioEnergyModelHelper.Install (apDevices, sources);
  radioEnergyModelHelper.Install (staDevices, sources1);

  Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource> (sources.Get (0));
  basicSourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  basicSourcePtr->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));

  Ptr<BasicEnergySource> basicSourcePtr1 = DynamicCast<BasicEnergySource> (sources1.Get (0));
  basicSourcePtr1->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  basicSourcePtr1->TraceConnectWithoutContext ("TotalEnergyConsumed", MakeCallback (&TotalEnergyConsumed));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}