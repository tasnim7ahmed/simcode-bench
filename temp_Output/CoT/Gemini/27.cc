#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nGoodput");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWlanNodes = 1;
  double simTime = 10;
  std::string rate ("5Mbps");
  double distance = 10;
  bool rtscts = false;
  bool udp = true;
  uint32_t packetSize = 1472;
  double expectedThroughput = 5;
  uint8_t mcs = 0;
  bool shortGuardInterval = false;
  uint32_t channelWidth = 20;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("rate", "CBR traffic rate", rate);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("rtscts", "Enable RTS/CTS", rtscts);
  cmd.AddValue ("udp", "Use UDP", udp);
  cmd.AddValue ("packetSize", "Packet size for CBR traffic", packetSize);
  cmd.AddValue ("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
  cmd.AddValue ("mcs", "MCS value (0-7)", mcs);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.AddValue ("channelWidth", "Channel Width (20 or 40)", channelWidth);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Wifi80211nGoodput", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (nWlanNodes);

  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  if (rtscts)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ এডমন্ডস::WifiMac/RtsCtsThreshold", StringValue ("0"));
    }

  Wifi80211nHelper wifi80211n;
  wifi80211n.SetMcs (mcs);
  if (shortGuardInterval)
    {
      wifi80211n.SetShortGuardInterval (true);
    }
  if (channelWidth == 40)
    {
      wifi80211n.Set40MHz ();
    }
  else
    {
      wifi80211n.Set20MHz ();
    }
  wifi.UpdateHeSettings (staDevices, wifi80211n);
  wifi.UpdateHeSettings (apDevices, wifi80211n);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);
  mobility.Install (apNode);

  InternetStackHelper internet;
  internet.Install (staNodes);
  internet.Install (apNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (staDevices);
  Ipv4InterfaceContainer iap = ipv4.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  ApplicationContainer app;
  if (udp)
    {
      UdpClientHelper client (iap.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Time ("0.000001"))); // packets/second
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      app = client.Install (staNodes.Get (0));

      UdpServerHelper server (port);
      ApplicationContainer serverApps = server.Install (apNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simTime + 1));
    }
  else
    {
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (iap.GetAddress (0), port));
      source.SetAttribute ("SendSize", UintegerValue (packetSize));
      app = source.Install (staNodes.Get (0));

      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApp = sink.Install (apNode.Get (0));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simTime + 1));
    }

  app.Start (Seconds (1.0));
  app.Stop (Seconds (simTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime + 5));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double goodput = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == iap.GetAddress (0))
        {
          goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << goodput << " Mbps\n";

          if (goodput < expectedThroughput * 0.9)
            {
              std::cout << " *** NOT достигнуто Throughput ***\n";
            }
        }
    }

  monitor->SerializeToXmlFile("wifi-80211n.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}