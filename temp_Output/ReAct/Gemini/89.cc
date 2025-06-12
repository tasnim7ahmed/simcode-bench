#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("EnergyHarvestingSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

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

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
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
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJoule", DoubleValue (1));

  EnergySourceContainer sources = basicSourceHelper.Install (nodes.Get (0));
  EnergySourceContainer sinkSources = basicSourceHelper.Install (nodes.Get (1));

  BasicEnergyHarvesterHelper basicHarvesterHelper;
  basicHarvesterHelper.Set ("HarvestedPower", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=0.1]"));
  basicHarvesterHelper.Install (nodes.Get (0), sources.Get (0));
  basicHarvesterHelper.Install (nodes.Get (1), sinkSources.Get (0));

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));

  radioEnergyModelHelper.Install (staDevices.Get (0), sinkSources.Get (0));
  radioEnergyModelHelper.Install (apDevices.Get (0), sources.Get (0));

  Simulator::Schedule (Seconds (0.0), [] () {
    NS_LOG_UNCOND ("Starting Simulation");
  });

  Ptr<BasicEnergySource> recvEnergySource = DynamicCast<BasicEnergySource> (sinkSources.Get (0));
  Simulator::Schedule (Seconds (1.0), [&recvEnergySource] () {
    NS_LOG_UNCOND ("Time, Remaining Energy (J), Power Harvester (W)");
  });

  for (uint32_t i = 1; i <= 10; ++i) {
      Simulator::Schedule (Seconds (i), [recvEnergySource, i] () {
        double energy = recvEnergySource->GetRemainingEnergy ();
        double power = recvEnergySource->GetEnergyHarvester()->GetCurrentHarvestedPower();
        NS_LOG_UNCOND (i << ", " << energy << ", " << power);
      });
  }


  Simulator::Stop (Seconds (10.0));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll (ascii.CreateFileStream ("energy-harvesting.tr"));
      phy.EnablePcapAll ("energy-harvesting");

      AnimationInterface anim ("energy-harvesting.xml");
      anim.EnablePacketMetadata ();
    }

  Simulator::Run ();

  double finalEnergySource0 = DynamicCast<BasicEnergySource>(sources.Get(0))->GetRemainingEnergy();
  double finalEnergySource1 = DynamicCast<BasicEnergySource>(sinkSources.Get(0))->GetRemainingEnergy();

  std::cout << "Final energy source 0: " << finalEnergySource0 << " Joules" << std::endl;
  std::cout << "Final energy source 1: " << finalEnergySource1 << " Joules" << std::endl;

  Simulator::Destroy ();
  return 0;
}