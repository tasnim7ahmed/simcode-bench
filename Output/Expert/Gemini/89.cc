#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("EnergyEfficientWireless");

double GenerateRandomHarvestedPower() {
    static std::default_random_engine generator;
    static std::uniform_real_distribution<double> distribution(0.0, 0.1);
    return distribution(generator);
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("energy-example");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (1.0)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (apDevices);
  i = ipv4.Assign (staDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (11.0));

  UdpEchoClientHelper echoClient (i.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (12.0));

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJoule", DoubleValue (1.0));

  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (0));
  EnergySourceContainer sinkSources = basicSourceHelper.Install (nodes.Get (1));

  BasicEnergyHarvesterHelper basicHarvesterHelper;
  basicHarvesterHelper.Set ("HarvestedPower", StringValue ("ns3::ConstantRandomVariable[Constant=0.05]"));
  EnergyHarvesterContainer harvesters = basicHarvesterHelper.Install (nodes.Get (0), sources);
  EnergyHarvesterContainer sinkHarvesters = basicHarvesterHelper.Install (nodes.Get (1), sinkSources);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  radioEnergyModelHelper.Install (staDevices, sinkSources);
  radioEnergyModelHelper.Install (apDevices, sources);

  Ptr<EnergySource> sinkSource = sinkSources.Get (0);

  Simulator::Schedule (Seconds (0.1), []() {
    NS_LOG_UNCOND("Starting Simulation");
  });

  Simulator::Schedule (Seconds (1.0), [&sinkSource]() {
        NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds () << " s: Energy " << sinkSource->GetCurrentEnergy () << " J");
  });
    
  for (int i = 1; i < 11; ++i) {
       Simulator::Schedule (Seconds (i), [&sinkSource]() {
         NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds () << " s: Energy " << sinkSource->GetCurrentEnergy () << " J");
       });
       Simulator::Schedule (Seconds (i), [&sinkHarvesters]() {
         Ptr<BasicEnergyHarvester> harvester = DynamicCast<BasicEnergyHarvester>(sinkHarvesters.Get(0));
         double harvestedPower = GenerateRandomHarvestedPower();
         harvester->SetHarvestedPower(harvestedPower);
       });
    }

  Simulator::Stop (Seconds (12.0));

  Simulator::Run ();

  double finalEnergy = sinkSource->GetCurrentEnergy ();
  NS_LOG_UNCOND ("Final Energy of node 1: " << finalEnergy << " J");

  Simulator::Destroy ();
  return 0;
}