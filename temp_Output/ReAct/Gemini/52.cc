#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTiming");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1472;
  std::string dataRate = "5Mbps";
  std::string phyMode = "Ht80MHz";
  double simulationTime = 10;
  double slotTime = 9e-6;
  double sifs = 16e-6;
  double pifs = 25e-6;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("packetSize", "Size of packets generated. Default: 1472", packetSize);
  cmd.AddValue ("dataRate", "Data rate for UDP client traffic (e.g., '5Mbps')", dataRate);
  cmd.AddValue ("phyMode", "802.11n PHY mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("slotTime", "Slot time in seconds", slotTime);
  cmd.AddValue ("sifs", "SIFS in seconds", sifs);
  cmd.AddValue ("pifs", "PIFS in seconds", pifs);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (1);
  NodeContainer apNodes;
  apNodes.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);
  mobility.Install (apNodes);

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (staNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client (staInterfaces.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time (Seconds (0.00001))));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client.SetDataRate (DataRate (dataRate));
  ApplicationContainer clientApps = client.Install (apNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue(Seconds(slotTime)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Sifs", TimeValue(Seconds(sifs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Pifs", TimeValue(Seconds(pifs)));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.destinationAddress == staInterfaces.GetAddress (0))
        {
		  std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
		  std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
		  std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
		  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000  << " Mbps\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}