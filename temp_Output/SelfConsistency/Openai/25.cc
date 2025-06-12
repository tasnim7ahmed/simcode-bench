/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * IEEE 802.11ax Wi-Fi configurable simulation
 * Allows manipulation of MCS, channel width, guard interval, band, number of stations, payload, flows, etc.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAxPerformanceSim");

void
PrintResults (Ptr<FlowMonitor> monitor, FlowMonitorHelper &flowmonHelper)
{
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::cout << "FlowId\tSrcAddr\t\tDstAddr\t\tProtocol\tRxBytes\tThroughput (Mbit/s)\tDelay (ms)\n";

  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::string proto = (t.protocol == 6 ? "TCP" : (t.protocol == 17 ? "UDP" : "Other"));
      double duration = (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ());
      double throughput = duration > 0 ? (flow.second.rxBytes * 8.0 / duration / 1e6) : 0;

      double meanDelayMs = flow.second.rxPackets > 0 ?
        (flow.second.delaySum.GetSeconds () / flow.second.rxPackets * 1e3) : 0.0;

      std::cout << flow.first << "\t"
                << t.sourceAddress << "\t"
                << t.destinationAddress << "\t"
                << proto << "\t\t"
                << flow.second.rxBytes << "\t"
                << throughput << "\t\t\t"
                << meanDelayMs << "\n";
    }
}

int
main (int argc, char *argv[])
{
  // Configurable parameters
  uint32_t nStations = 4;
  double distance = 20.0;
  uint32_t mcs = 9;
  uint32_t channelWidth = 80;
  bool shortGuardInterval = true;
  bool useExtendedBlockAck = false;
  bool enableRtsCts = false;
  bool use80_80MHz = false;
  bool useSpectrumPhy = false;
  bool enableUlOfdma = false;
  std::string ackSequence = "normal";
  std::string phyBand = "5GHz"; // options: 2.4GHz, 5GHz, 6GHz
  bool useUdp = true;
  uint32_t payloadSize = 1472;
  double offeredLoadMbps = 100.0;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nStations", "Number of Wi-Fi stations", nStations);
  cmd.AddValue ("distance", "Distance between AP and stations (meters)", distance);
  cmd.AddValue ("mcs", "MCS value to use", mcs);
  cmd.AddValue ("channelWidth", "Channel width (MHz): 20, 40, 80, 160", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
  cmd.AddValue ("useExtendedBlockAck", "Enable IEEE 802.11ax Extended Block Ack", useExtendedBlockAck);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue ("use80_80MHz", "Enable 80+80 MHz channels", use80_80MHz);
  cmd.AddValue ("useSpectrumPhy", "Use SpectrumWifiPhyModel (true), else YansWifiPhyModel", useSpectrumPhy);
  cmd.AddValue ("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
  cmd.AddValue ("phyBand", "Frequency band: 2.4GHz, 5GHz, 6GHz", phyBand);
  cmd.AddValue ("ackSequence", "DL MU PPDU ack sequence: normal/other", ackSequence);
  cmd.AddValue ("useUdp", "Use UDP (true) or TCP (false)", useUdp);
  cmd.AddValue ("payloadSize", "Packet payload size in bytes", payloadSize);
  cmd.AddValue ("offeredLoadMbps", "Target/Application total offered load in Mbps", offeredLoadMbps);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.Parse (argc, argv);

  // Log params
  NS_LOG_UNCOND ("nStations: " << nStations
                  << ", distance: " << distance
                  << "m, MCS: " << mcs
                  << ", ChannelWidth: " << channelWidth
                  << "MHz, ShortGI: " << (shortGuardInterval ? "Y" : "N")
                  << ", UseExtendedBlockAck: " << (useExtendedBlockAck ? "Y" : "N")
                  << ", enableRtsCts: " << (enableRtsCts ? "Y" : "N")
                  << ", use80_80MHz: " << (use80_80MHz ? "Y" : "N")
                  << ", useSpectrumPhy: " << (useSpectrumPhy ? "Y" : "N")
                  << ", enableUlOfdma: " << (enableUlOfdma ? "Y" : "N")
                  << ", ackSequence: " << ackSequence
                  << ", phyBand: " << phyBand
                  << ", useUdp: " << (useUdp ? "UDP" : "TCP")
                  << ", payloadSize: " << payloadSize << " bytes"
                  << ", offeredLoad: " << offeredLoadMbps << " Mbps"
                  << ", SimTime: " << simulationTime << " s"
  );

  // 1. Create nodes: 1 AP + n stations
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nStations);
  NodeContainer allNodes;
  allNodes.Add (wifiApNode);
  allNodes.Add (wifiStaNodes);

  // 2. Configure Wi-Fi PHY and channel
  Ptr<YansWifiChannel> yansChannel;
  Ptr<SpectrumWifiPhy> spectrumPhy = nullptr;
  Ptr<YansWifiPhy> yansPhy = nullptr;
  Ptr<WifiChannel> wifiChannel = nullptr;

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  SpectrumWifiPhyHelper spectrumPhyHelper;
  YansWifiPhyHelper yansPhyHelper = YansWifiPhyHelper::Default ();
  WifiMacHelper wifiMacHelper;
  WifiHelper wifiHelper;

  // Choose band/channel
  uint16_t channelNumber = 36; // 5GHz default
  double centerFrequency = 5180.0; // MHz, default for ch36
  if (phyBand == "2.4GHz")
    {
      channelNumber = 6;
      centerFrequency = 2437.0;
    }
  else if (phyBand == "5GHz")
    {
      channelNumber = 36;
      centerFrequency = 5180.0;
    }
  else if (phyBand == "6GHz")
    {
      channelNumber = 1; // 5955 MHz, ch1 in 6GHz
      centerFrequency = 5955.0;
    }
  else
    {
      NS_ABORT_MSG ("Invalid phyBand: " << phyBand);
    }

  // Configure Wi-Fi 802.11ax
  wifiHelper.SetStandard (phyBand == "6GHz" ? WIFI_STANDARD_80211ax_6GHZ : 
                                          (phyBand == "5GHz" ? WIFI_STANDARD_80211ax : WIFI_STANDARD_80211ax));

  // Configuration of WifiPhy
  if (useSpectrumPhy)
    {
      SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
      spectrumPhyHelper = phy;

      spectrumPhyHelper.SetChannel (SpectrumChannelHelper::Default ().Create ());
      spectrumPhyHelper.Set ("ChannelNumber", UintegerValue (channelNumber));

      // Set frequency (if needed)
      spectrumPhyHelper.Set ("Frequency", UintegerValue (static_cast<uint32_t>(centerFrequency * 1e6))); // in Hz

      wifiChannel = spectrumPhyHelper.GetChannel ();
    }
  else
    {
      yansChannel = channelHelper.Create ();
      yansPhyHelper.SetChannel (yansChannel);
      yansPhyHelper.Set ("ChannelNumber", UintegerValue (channelNumber));
      yansPhyHelper.Set ("Frequency", UintegerValue (static_cast<uint32_t>(centerFrequency * 1e6))); // in Hz
      wifiChannel = yansPhyHelper.GetChannel ();
    }

  // Configure AP and STA MAC
  Ssid ssid = Ssid ("wifi-ax");

  wifiMacHelper.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "ActiveProbing", BooleanValue (false),
                         "QosSupported", BooleanValue (true));

  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;

  if (useSpectrumPhy)
    {
      // Not all spectrum/phy features are implemented; main difference for 802.11ax is PHY modeling
      staDevices = wifiHelper.Install (spectrumPhyHelper, wifiMacHelper, wifiStaNodes);
      wifiMacHelper.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevice = wifiHelper.Install (spectrumPhyHelper, wifiMacHelper, wifiApNode);
    }
  else
    {
      staDevices = wifiHelper.Install (yansPhyHelper, wifiMacHelper, wifiStaNodes);
      wifiMacHelper.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevice = wifiHelper.Install (yansPhyHelper, wifiMacHelper, wifiApNode);
    }

  // Set guard interval and channel/phy options
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (shortGuardInterval));

  if (use80_80MHz)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/EnableHe80P80", BooleanValue (true));
    }

  // Set MCS for all SSs and AP
  std::ostringstream oss;
  oss << "HeMcs" << mcs;
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (oss.str ()),
                                      "ControlMode", StringValue ("HeMcs0")); // robust control

  // Enable/Disable RTS/CTS
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", 
               UintegerValue (enableRtsCts ? 0 : 999999));

  // Extended Block Ack (not all ns-3 modules support this directly, may need patch, but we set attribute if available)
  if (useExtendedBlockAck)
    {
      Config::SetDefault ("ns3::HeConfiguration::BlockAck", BooleanValue (true));
    }

  // UL OFDMA: ns-3 support is limited, but we can enable OFDMA mode attributes for examination
  if (enableUlOfdma)
    {
      Config::SetDefault ("ns3::HeConfiguration::UlOfdma", BooleanValue (true));
    }

  // Schedule channel access: ns-3 does not emulate wireless scheduling per se, but application start can be staggered if needed

  // 3. Mobility: Place AP and STAs at fixed distance
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 1.5)); // AP

  for (uint32_t i = 0; i < nStations; ++i)
    {
      double angle = (2 * M_PI / nStations) * i;
      double x = distance * std::cos (angle);
      double y = distance * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 1.0));
    }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // 4. Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  // 5. Applications: Each STA is a sink; AP sends DL flows to each STA
  ApplicationContainer serverApps, clientApps;
  uint16_t port = 5000;
  double appStart = 1.0;
  double appStop = simulationTime - 1.0;

  if (useUdp)
    {
      // UdpServer on each STA, UdpClient on AP
      for (uint32_t i = 0; i < nStations; ++i)
        {
          UdpServerHelper server (port + i);
          serverApps.Add (server.Install (wifiStaNodes.Get (i)));
        }
      serverApps.Start (Seconds (appStart));
      serverApps.Stop (Seconds (appStop));

      for (uint32_t i = 0; i < nStations; ++i)
        {
          double staLoadMbps = offeredLoadMbps / double (nStations);
          UdpClientHelper client (staInterfaces.GetAddress (i), port + i);
          client.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
          client.SetAttribute ("Interval", TimeValue (Seconds (payloadSize * 8.0 / (staLoadMbps * 1e6))));
          client.SetAttribute ("PacketSize", UintegerValue (payloadSize));

          clientApps.Add (client.Install (wifiApNode.Get (0)));
        }
      clientApps.Start (Seconds (appStart + 0.5));
      clientApps.Stop (Seconds (appStop - 0.5));
    }
  else
    {
      // TCP: PacketSink on STA, BulkSendApplication on AP
      uint32_t tcpSegmentSize = payloadSize;
      Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcpSegmentSize));
      for (uint32_t i = 0; i < nStations; ++i)
        {
          Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port + i));
          PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
          serverApps.Add (sinkHelper.Install (wifiStaNodes.Get (i)));
        }
      serverApps.Start (Seconds (appStart));
      serverApps.Stop (Seconds (appStop));

      for (uint32_t i = 0; i < nStations; ++i)
        {
          BulkSendHelper bulkSend ("ns3::TcpSocketFactory",
                                   InetSocketAddress (staInterfaces.GetAddress (i), port + i));
          bulkSend.SetAttribute ("MaxBytes", UintegerValue (0));
          bulkSend.SetAttribute ("SendSize", UintegerValue (tcpSegmentSize));
          clientApps.Add (bulkSend.Install (wifiApNode.Get (0)));
        }
      clientApps.Start (Seconds (appStart + 0.5));
      clientApps.Stop (Seconds (appStop - 0.5));
    }

  // 6. Flow Monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  // 7. Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // 8. Report results
  PrintResults (monitor, flowmonHelper);

  Simulator::Destroy ();
  return 0;
}