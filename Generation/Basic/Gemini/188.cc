#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

// Define logging for this simulation program
NS_LOG_COMPONENT_DEFINE ("WifiUdpNetAnimExample");

int main (int argc, char *argv[])
{
  // 1. Configure simulation parameters
  double simulationTime = 10.0; // seconds
  uint32_t packetSize = 1024;   // bytes
  std::string dataRate = "1Mbps"; // OnOffApplication data rate

  // 2. Command line arguments for customization
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Total duration of the simulation in seconds", simulationTime);
  cmd.AddValue ("packetSize", "UDP packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "OnOffApplication data rate", dataRate);
  cmd.Parse (argc, argv);

  // Set default values for global attributes for OnOffApplication
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (packetSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));

  // 3. Create nodes: One AP and two STA nodes
  NS_LOG_INFO ("Create nodes.");
  NodeContainer wifiApNode;
  wifiApNode.Create (1); // Access Point node
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2); // Two Station nodes

  // 4. Configure mobility (fixed positions for NetAnim visualization)
  NS_LOG_INFO ("Install Mobility Model.");
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  // Set fixed positions for visual clarity in NetAnim
  Ptr<ConstantPositionMobilityModel> apMobility = wifiApNode.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  apMobility->SetPosition (Vector (0.0, 0.0, 0.0)); // AP at origin

  Ptr<ConstantPositionMobilityModel> sta1Mobility = wifiStaNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  sta1Mobility->SetPosition (Vector (5.0, 0.0, 0.0)); // STA1 (sender)

  Ptr<ConstantPositionMobilityModel> sta2Mobility = wifiStaNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  sta2Mobility->SetPosition (Vector (-5.0, 0.0, 0.0)); // STA2 (receiver)

  // 5. Configure Wi-Fi devices
  NS_LOG_INFO ("Configure Wifi.");
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_STANDARD_80211n); // Using 802.11n standard

  YansWifiPhyHelper phy;
  // Set up the channel with propagation loss and channel models
  YansWifiChannelHelper channel;
  channel.SetPropagationLossModel ("ns3::FriisPropagationLossModel"); // Simple path loss model
  channel.SetChannelModel ("ns3::YansWifiChannel"); // Simple Wi-Fi channel model
  phy.SetChannel (channel.Create ());
  phy.SetErrorRateModel ("ns3::YansErrorRateModel"); // Simple error model for PHY
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO); // Required for Wireshark captures (if enabled)

  // Configure MAC layer for AP and STAs
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-network"); // Network SSID

  // AP MAC configuration
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MicroSeconds (102400))); // Default beacon interval
  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install (phy, mac, wifiApNode);

  // STA MAC configuration
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false)); // STAs do not actively probe for AP
  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (phy, mac, wifiStaNodes);

  // 6. Install Internet Stack (IPv4)
  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0"); // Assign IP addresses from this subnet

  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices); // STA1 gets 10.1.1.2, STA2 gets 10.1.1.3

  // 7. Configure Applications (UDP Client/Server)
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9; // Standard discard port for PacketSink

  // Setup PacketSink (UDP server) on STA2 (wifiStaNodes.Get(1))
  Address sinkAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (wifiStaNodes.Get (1)); // STA2 is the receiver
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 0.1)); // Stop slightly after simulation ends

  // Setup OnOffApplication (UDP client) on STA1 (wifiStaNodes.Get(0))
  OnOffHelper onoffHelper ("ns3::UdpSocketFactory", Address ()); // Remote address set later
  onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // DataRate and PacketSize are set by default attributes configured earlier

  // Set the remote address to STA2's IP address (the PacketSink)
  Address clientRemoteAddress (InetSocketAddress (staInterfaces.GetAddress (1), port));
  onoffHelper.SetAttribute ("Remote", AddressValue (clientRemoteAddress));

  ApplicationContainer clientApps = onoffHelper.Install (wifiStaNodes.Get (0)); // STA1 is the sender
  clientApps.Start (Seconds (1.0)); // Start sending after 1 second
  clientApps.Stop (Seconds (simulationTime)); // Stop sending at simulation end

  // 8. Enable NetAnim visualization
  NS_LOG_INFO ("Enable NetAnim visualization.");
  AnimationInterface anim ("wifi-udp-netanim.xml");
  // Set positions for NetAnim; these match the mobility model positions
  anim.SetConstantPosition (wifiApNode.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (wifiStaNodes.Get (0), 5.0, 0.0);
  anim.SetConstantPosition (wifiStaNodes.Get (1), -5.0, 0.0);

  // Optional: Update node descriptions and colors for better visualization
  anim.UpdateNodeDescription (wifiApNode.Get (0), "AP");
  anim.UpdateNodeColor (wifiApNode.Get (0), 255, 0, 0); // Red for AP
  anim.UpdateNodeDescription (wifiStaNodes.Get (0), "STA1 (Sender)");
  anim.UpdateNodeColor (wifiStaNodes.Get (0), 0, 0, 255); // Blue for STA1
  anim.UpdateNodeDescription (wifiStaNodes.Get (1), "STA2 (Receiver)");
  anim.UpdateNodeColor (wifiStaNodes.Get (1), 0, 255, 0); // Green for STA2

  // 9. Enable FlowMonitor for performance metrics
  NS_LOG_INFO ("Enable FlowMonitor.");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll (); // Install on all nodes

  // 10. Run Simulation
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (simulationTime + 0.5)); // Stop a bit after applications finish
  Simulator::Run ();

  // 11. Process and display FlowMonitor statistics
  NS_LOG_INFO ("Generate FlowMonitor statistics.");
  monitor->CheckFor (Seconds (simulationTime)); // Collect statistics at the end of simulation
  monitor->SerializeToXmlFile ("wifi-udp-flowmonitor.xml", true, true); // Save to XML for further analysis

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->Get
  stats ();

  double totalThroughputMbps = 0;
  uint64_t totalPacketsTx = 0;
  uint64_t totalPacketsRx = 0;
  uint64_t totalLostPackets = 0;

  // Iterate over all flows to find the UDP flow from STA1 to STA2
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->GetFiveTuple (i->first);

      // Identify the specific flow from STA1 (source 10.1.1.2) to STA2 (destination 10.1.1.3) on UDP port 9
      if (t.sourceAddress == staInterfaces.GetAddress (0) &&
          t.destinationAddress == staInterfaces.GetAddress (1) &&
          t.sourcePort == port &&
          t.protocol == 17) // 17 is UDP protocol number
        {
          NS_LOG_UNCOND ("\n--- Flow Details (STA1 -> STA2) ---");
          NS_LOG_UNCOND ("  Flow ID: " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")");
          NS_LOG_UNCOND ("  Tx Packets: " << i->second.txPackets);
          NS_LOG_UNCOND ("  Rx Packets: " << i->second.rxPackets);
          NS_LOG_UNCOND ("  Lost Packets: " << i->second.lostPackets);
          NS_LOG_UNCOND ("  Tx Bytes: " << i->second.txBytes);
          NS_LOG_UNCOND ("  Rx Bytes: " << i->second.rxBytes);

          // Calculate throughput for this flow
          double flowDuration = i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          if (flowDuration > 0)
            {
              double currentThroughput = (i->second.rxBytes * 8.0) / (flowDuration * 1000000.0); // in Mbps
              NS_LOG_UNCOND ("  Throughput: " << currentThroughput << " Mbps");
              totalThroughputMbps += currentThroughput;
            }
          else
            {
              NS_LOG_UNCOND ("  Throughput: 0 Mbps (No packets received)");
            }

          totalPacketsTx += i->second.txPackets;
          totalPacketsRx += i->second.rxPackets;
          totalLostPackets += i->second.lostPackets;
        }
    }

  NS_LOG_UNCOND ("\n--- Overall Performance Metrics ---");
  NS_LOG_UNCOND ("Total Packets Transmitted (STA1): " << totalPacketsTx);
  NS_LOG_UNCOND ("Total Packets Received (STA2):    " << totalPacketsRx);
  NS_LOG_UNCOND ("Total Packets Lost:               " << totalLostPackets);
  NS_LOG_UNCOND ("Total Throughput (STA1 -> STA2):  " << totalThroughputMbps << " Mbps");
  if (totalPacketsTx > 0)
    {
      NS_LOG_UNCOND ("Packet Loss Rate:                 " << (double)totalLostPackets / totalPacketsTx * 100.0 << " %");
    }
  else
    {
      NS_LOG_UNCOND ("Packet Loss Rate:                 0.0 % (No packets transmitted)");
    }

  // 12. Cleanup
  NS_LOG_INFO ("Done.");
  Simulator::Destroy ();

  return 0;
}