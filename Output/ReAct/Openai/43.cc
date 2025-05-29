#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHiddenNodeMpduAggregation");

int
main (int argc, char *argv[])
{
  // User-configurable parameters
  bool enableRtsCts = false;
  uint32_t maxAmpdu = 16;   // Number of MPDUs in an A-MPDU
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double expectedThroughput = 10.0; // Mbps

  CommandLine cmd;
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
  cmd.AddValue ("maxAmpdu", "Maximum A-MPDU size (number of MPDUs)", maxAmpdu);
  cmd.AddValue ("payloadSize", "UDP payload size in bytes", payloadSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("expectedThroughput", "Expected throughput lower bound in Mbps", expectedThroughput);
  cmd.Parse (argc, argv);

  // Network topology:
  // STA1 and STA2 placed 10m apart, both within 5m of AP, but not with each other
  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (2);
  wifiApNode.Create (1);

  // WiFi PHY/channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue(20.0));
  phy.Set ("TxPowerEnd", DoubleValue(20.0));
  // Short range
  phy.Set ("EnergyDetectionThreshold", DoubleValue(-65.0));
  phy.Set ("CcaMode1Threshold", DoubleValue(-62.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  // MPDU aggregation parameters
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxAmsduSize", UintegerValue(7935));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue (maxAmpdu * payloadSize));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("HtMcs7"),
                               "ControlMode", StringValue ("HtMcs0"),
                               "RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 2347));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility: AP at origin, STA1 at (5,0,0), STA2 at (-5,0,0)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));    // AP
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));    // STA1
  positionAlloc->Add (Vector (-5.0, 0.0, 0.0));   // STA2
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  // Internet stack + IP addresses
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterface;
  staInterfaces = address.Assign (staDevices);
  apInterface = address.Assign (apDevice);

  // UDP server (AP)
  uint16_t port = 5000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1.0));

  // UDP clients (saturated traffic from both STAs)
  UdpClientHelper client0 (apInterface.GetAddress (0), port);
  client0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  client0.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  client0.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));

  UdpClientHelper client1 (apInterface.GetAddress (0), port);
  client1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  client1.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  client1.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));

  ApplicationContainer clientApps;
  clientApps.Add (client0.Install (wifiStaNodes.Get (0)));
  clientApps.Add (client1.Install (wifiStaNodes.Get (1)));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1.0));

  // Enable PCAP and ASCII traces
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcapAll ("hidden-80211n-aggregate", false);
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("hidden-80211n-aggregate.tr"));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 2.0));
  Simulator::Run ();

  // Throughput calculation (aggregate flows from both stations)
  double totalThroughputMbps = 0.0;
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      // Only consider flows to AP
      if (t.destinationAddress == apInterface.GetAddress (0) && t.destinationPort == port)
        {
          double rxBytes = iter->second.rxBytes * 8.0;
          double tput = rxBytes / (simulationTime * 1e6); // Mbps
          totalThroughputMbps += tput;
        }
    }
  std::cout << "Aggregate Throughput: " << totalThroughputMbps << " Mbps" << std::endl;
  if (totalThroughputMbps < expectedThroughput)
    {
      NS_LOG_UNCOND("ERROR: Aggregate throughput " << totalThroughputMbps << " Mbps is less than expected " << expectedThroughput << " Mbps");
      return 1;
    }

  Simulator::Destroy ();
  return 0;
}