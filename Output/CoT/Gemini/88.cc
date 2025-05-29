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
#include "ns3/on-off-application.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergy");

static void
RemainingEnergy (double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND ("Remaining Energy = " << remainingEnergy << " J");
}

static void
TotalEnergyConsumed (double oldValue, double totalEnergyConsumed)
{
  NS_LOG_UNCOND ("Total Energy Consumed = " << totalEnergyConsumed << " J");
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  double startTime = 1.0;
  double nodeDistance = 10.0;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("packetSize", "Size of packet sent", packetSize);
  cmd.AddValue ("startTime", "Start time for application", startTime);
  cmd.AddValue ("nodeDistance", "Distance between nodes", nodeDistance);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
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
  staDevices = wifi.Install (phy, mac, nodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (1));

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
  Ipv4InterfaceContainer interfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (startTime));
  clientApps.Stop (Seconds (simulationTime));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (0));
  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.00185));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.00000925));
  radioEnergyModelHelper.Install (staDevices.Get (0), sources.Get (0));

  Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource> (sources.Get (0));
  basicSourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
  basicSourcePtr->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergyConsumed));

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}