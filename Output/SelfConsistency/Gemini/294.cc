#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aarf-wifi-manager.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 5;
  double simulationTime = 10;
  std::string phyMode ("OfdmRate6Mbps");

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer wifiNodes;
  wifiNodes.Create (nWifi);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  Ssid ssid = Ssid ("ns-3-adhoc");
  wifiMac.SetSsid (ssid);

  WifiNetDeviceContainer wifiDevices = wifi.Install (wifiPhy, wifiMac, wifiNodes);

  /* Setting mobility model */
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  mobility.Install (wifiNodes);

  AarfWifiManager aarf; // Instantiate AarfWifiManager

  InternetStackHelper internet;
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (wifiDevices);

  uint16_t port = 9; // well-known echo port number
  UdpServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (wifiNodes.Get (nWifi - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper echoClient (i.GetAddress (nWifi - 1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (wifiNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Bytes: " << i->second.lostBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}