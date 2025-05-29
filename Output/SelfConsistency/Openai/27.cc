/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nGoodputExample");

static void
PrintUsage (char *prog)
{
  std::cout << "Usage: " << prog << " [Options]\n"
      "Options:\n"
      "  --simulationTime=SECONDS      Simulation time (default: 10)\n"
      "  --distance=METERS             Distance between STA and AP (default: 5)\n"
      "  --udp                        Use UDP instead of TCP (default: TCP)\n"
      "  --rts                        Enable RTS/CTS (default: false)\n"
      "  --channelWidth=MHZ           Channel width: 20 or 40 (default: 20)\n"
      "  --shortGuardInterval         Use short guard interval (default: long)\n"
      "  --mcs=VALUE                  MCS index (0..7, default: 7)\n"
      "  --expectedThroughput=Mbps    Expected throughput (for flows, default: 50)\n"
      "  --help                       Show this help message\n";
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0;         // seconds
  double distance = 5.0;                // meters
  bool udp = false;
  bool rtsCts = false;
  uint32_t channelWidth = 20;           // MHz
  bool shortGuardInterval = false;
  uint8_t mcs = 7;                      // 0..7 for HT-MCS
  double expectedThroughput = 50.0;     // Mbps

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", distance);
  cmd.AddValue ("udp", "Use UDP instead of TCP", udp);
  cmd.AddValue ("rts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue ("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval", shortGuardInterval);
  cmd.AddValue ("mcs", "MCS Index (0..7)", mcs);
  cmd.AddValue ("expectedThroughput", "Expected application throughput (Mbps)", expectedThroughput);
  cmd.AddValue ("help", "Print help", false);
  cmd.Parse (argc, argv);

  if (cmd.GetArgument ("help"))
    {
      PrintUsage (argv[0]);
      return 0;
    }

  if (mcs > 7)
    {
      NS_FATAL_ERROR ("MCS must be in range 0..7 for IEEE 802.11n single spatial stream");
    }
  if (channelWidth != 20 && channelWidth != 40)
    {
      NS_FATAL_ERROR ("Channel width must be 20 or 40 MHz");
    }

  // Log selected parameters
  std::cout << "\nSimulation parameters:" << std::endl;
  std::cout << "  Simulation time:    " << simulationTime << " s" << std::endl;
  std::cout << "  Distance:           " << distance << " m" << std::endl;
  std::cout << "  Mode:               " << (udp ? "UDP" : "TCP") << std::endl;
  std::cout << "  RTS/CTS:            " << (rtsCts ? "enabled" : "disabled") << std::endl;
  std::cout << "  Channel width:      " << channelWidth << " MHz" << std::endl;
  std::cout << "  Guard interval:     " << (shortGuardInterval ? "short" : "long") << std::endl;
  std::cout << "  MCS:                " << unsigned(mcs) << std::endl;
  std::cout << "  App throughput:     " << expectedThroughput << " Mbps" << std::endl;

  // Create nodes: 1 AP, 1 STA
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelWidth", UintegerValue (channelWidth));
  phy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
  phy.Set ("Antennas", UintegerValue (1));
  phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (1));
  phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (1));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;

  // Configure HT (High Throughput) and set MCS
  HtWifiMacHelper htMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("HtMcs" + std::to_string(mcs)),
                                "ControlMode", StringValue ("HtMcs0"),
                                "RtsCtsThreshold", UintegerValue (rtsCts ? 0 : 2347));

  Ssid ssid = Ssid ("ns3-80211n-sim");

  // STA
  htMac.SetType ("ns3::StaWifiMac",
                  "Ssid", SsidValue (ssid),
                  "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, htMac, wifiStaNode);

  // AP
  htMac.SetType ("ns3::ApWifiMac",
                  "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, htMac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));         // AP position
  positionAlloc->Add (Vector (distance, 0.0, 0.0));    // STA position
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  // IP assignment
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  NetDeviceContainer allDevices;
  allDevices.Add (apDevice);
  allDevices.Add (staDevice);
  Ipv4InterfaceContainer interfaces = address.Assign (allDevices);

  // Set up traffic application: STA -> AP (uplink)
  ApplicationContainer serverApp, clientApp;

  uint16_t port = 5001;
  if (udp)
    {
      // UDP Server on AP
      UdpServerHelper udpServer (port);
      serverApp = udpServer.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simulationTime + 1));

      // UDP Client on STA
      UdpClientHelper udpClient (interfaces.GetAddress (0), port);
      // Set client attributes
      udpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.0005))); // 2k packets/s
      udpClient.SetAttribute ("PacketSize", UintegerValue (1472));
      udpClient.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      udpClient.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      udpClient.SetAttribute ("DataRate", DataRateValue (DataRate((expectedThroughput * 1e6))));
      clientApp = udpClient.Install (wifiStaNode.Get (0));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simulationTime + 1));
    }
  else
    {
      // TCP Server on AP
      Address bindAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", bindAddress);
      serverApp = packetSinkHelper.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simulationTime + 1));

      // TCP Client (OnOff) on STA
      OnOffHelper onOffHelper ("ns3::TcpSocketFactory",
                               Address (InetSocketAddress (interfaces.GetAddress (0), port)));
      onOffHelper.SetAttribute ("DataRate", DataRateValue (DataRate (expectedThroughput * 1e6)));
      onOffHelper.SetAttribute ("PacketSize", UintegerValue (1448));
      onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onOffHelper.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      clientApp = onOffHelper.Install (wifiStaNode.Get (0));
    }

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // FlowMonitor stats (find the STA->AP flow)
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  double goodput = 0;

  for (auto const & flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      // Find the flow from STA to AP
      if (t.sourceAddress == interfaces.GetAddress (1) &&
          t.destinationAddress == interfaces.GetAddress (0))
        {
          double transmittedBytes = (double) flow.second.rxBytes;
          goodput = transmittedBytes * 8 / simulationTime / 1e6; // Mbps
          std::cout << "\nGoodput (STA->AP):    " << std::fixed << std::setprecision (4) << goodput << " Mbps" << std::endl;
          std::cout << "  Packets received:   " << flow.second.rxPackets << std::endl;
          std::cout << "  Packets lost:       " << flow.second.lostPackets << std::endl;
          std::cout << "  Time first packet:  " << flow.second.timeFirstRxPacket.As (Time::S) << " s" << std::endl;
          std::cout << "  Time last packet:   " << flow.second.timeLastRxPacket.As (Time::S) << " s" << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}