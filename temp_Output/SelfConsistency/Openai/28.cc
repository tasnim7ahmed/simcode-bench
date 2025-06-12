/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Example: Mixed 802.11b/g/n (HT and non-HT) station simulation
 * Compares performance under multiple protection and physical layer scenarios,
 * for both UDP and TCP, and shows throughput results for each config.
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWifiHtExample");

// Utility function to calculate throughput from Flow Monitor results
void
PrintThroughput (Ptr<FlowMonitor> flowMonitor)
{
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
  for (auto const &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t =
        DynamicCast<Ipv4FlowClassifier>(flowMonitor->GetClassifier ())->FindFlow (flow.first);
      double throughput = (flow.second.rxBytes * 8.0 / 1e6) / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()); // Mbps
      std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << " (proto " << int (t.protocol)
                << "): " << throughput << " Mbps (" << flow.second.rxPackets << " rx packets)" << std::endl;
    }
}

int main (int argc, char *argv[])
{
  // Simulation parameters
  double simTime = 10.0; // seconds
  uint32_t payloadSize = 1472; // bytes, default UDP
  std::string offeredLoad = "54Mbps";
  bool useShortSlot = true;
  bool useErpProtection = true; // enables ERP protection for 11b
  bool useShortPreamble = true;
  bool useShortGuardInterval = false;
  bool useShortPpdu = false;
  bool useTcp = false;
  uint32_t nB = 1;   // # 802.11b
  uint32_t nG = 1;   // # 802.11g (non-HT)
  uint32_t nN = 1;   // # 802.11n (HT)

  CommandLine cmd;
  cmd.AddValue ("useTcp", "UDP (false) or TCP (true)", useTcp);
  cmd.AddValue ("payloadSize", "Application payload size in bytes", payloadSize);
  cmd.AddValue ("offeredLoad", "UdpClient offered rate", offeredLoad);
  cmd.AddValue ("nB", "Number of 802.11b stations", nB);
  cmd.AddValue ("nG", "Number of 802.11g (non-HT) stations", nG);
  cmd.AddValue ("nN", "Number of 802.11n (HT) stations", nN);
  cmd.AddValue ("useShortSlot", "Short slot time (true: 9us, false: 20us)", useShortSlot);
  cmd.AddValue ("useErpProtection", "Enable ERP protection", useErpProtection);
  cmd.AddValue ("useShortPreamble", "Use short preamble/short PPDU (ERP, HT)", useShortPreamble);
  cmd.AddValue ("useShortGuardInterval", "HT: use short GI", useShortGuardInterval);
  cmd.AddValue ("useShortPpdu", "HT: use short PPDU format", useShortPpdu);
  cmd.Parse (argc, argv);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");

  // AP node
  NodeContainer apNode;
  apNode.Create (1);

  // Create stations
  NodeContainer staBNodes, staGNodes, staNNodes;
  staBNodes.Create (nB);
  staGNodes.Create (nG);
  staNNodes.Create (nN);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
  for (uint32_t i = 0; i < nB + nG + nN; ++i)
    {
      positionAlloc->Add (Vector (5.0 * (i + 1), 0.0, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);
  mobility.Install (staBNodes);
  mobility.Install (staGNodes);
  mobility.Install (staNNodes);

  // SSID
  Ssid ssid = Ssid ("wifi-mixed");

  // ----- 11b -----
  WifiHelper wifiB;
  wifiB.SetStandard (WIFI_STANDARD_80211b);
  // Long/short preamble
  wifiB.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("DsssRate11Mbps"),
                                  "ControlMode", StringValue ("DsssRate11Mbps"));
  YansWifiPhyHelper phyB = YansWifiPhyHelper::Default ();
  phyB.SetChannel (wifiChannel.Create ());
  WifiMacHelper macB;
  macB.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid),
                "ActiveProbing", BooleanValue (false),
                "ShortPreambleSupported", BooleanValue (useShortPreamble));
  NetDeviceContainer staBDevices = wifiB.Install (phyB, macB, staBNodes);

  // ----- 11g (ERP) -----
  WifiHelper wifiG;
  wifiG.SetStandard (WIFI_STANDARD_80211g);
  wifiG.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("ErpOfdmRate54Mbps"),
                                  "ControlMode", StringValue ("ErpOfdmRate6Mbps"));
  YansWifiPhyHelper phyG = YansWifiPhyHelper::Default ();
  phyG.SetChannel (wifiChannel.Create ());
  if (!useShortSlot)
    {
      Config::SetDefault ("ns3::WifiPhy::ShortSlotTimeSupported", BooleanValue (false));
    }
  macB.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid),
                "ActiveProbing", BooleanValue (false),
                "ShortPreambleSupported", BooleanValue (useShortPreamble));
  NetDeviceContainer staGDevices = wifiG.Install (phyG, macB, staGNodes);

  // ----- 11n (HT) -----
  WifiHelper wifiN;
  wifiN.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
  wifiN.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs7"),
                                  "ControlMode", StringValue ("HtMcs0"));
  YansWifiPhyHelper phyN = YansWifiPhyHelper::Default ();
  phyN.SetChannel (wifiChannel.Create ());
  // PPDU format & short GI for HT
  phyN.Set ("ShortGuardEnabled", BooleanValue (useShortGuardInterval));
  phyN.Set ("GreenfieldEnabled", BooleanValue (useShortPpdu));
  WifiMacHelper macN;
  macN.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid),
                "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staNDevices = wifiN.Install (phyN, macN, staNNodes);

  // ----- AP -----
  // AP supports all standards
  WifiHelper wifiAp;
  wifiAp.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
  wifiAp.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs7"),
                                  "ControlMode", StringValue ("HtMcs0"));
  YansWifiPhyHelper phyAp = YansWifiPhyHelper::Default ();
  phyAp.SetChannel (wifiChannel.Create ());
  WifiMacHelper macAp;
  macAp.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "EnableNonErpProtection", BooleanValue (useErpProtection),
                 "ShortPreambleSupported", BooleanValue (useShortPreamble));
  NetDeviceContainer apDevice = wifiAp.Install (phyAp, macAp, apNode);

  // ----- Internet stack -----
  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staBNodes);
  stack.Install (staGNodes);
  stack.Install (staNNodes);

  // Addressing
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  std::vector<Ipv4InterfaceContainer> staInterfaces;

  // Assign IP addresses
  NetDeviceContainer allStaDevices;
  allStaDevices.Add (staBDevices);
  allStaDevices.Add (staGDevices);
  allStaDevices.Add (staNDevices);

  Ipv4InterfaceContainer apIf = address.Assign (apDevice);
  Ipv4InterfaceContainer staIfs = address.Assign (allStaDevices);

  // ----- Applications -----
  uint16_t portUdp = 4000;
  uint16_t portTcp = 5000;

  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  // For UDP:
  for (uint32_t i = 0; i < allStaDevices.GetN (); ++i)
    {
      Ptr<Node> sta = NodeList::GetNode (staBNodes.GetN () + staGNodes.GetN () + i);

      if (!useTcp)
        {
          UdpServerHelper udpServer (portUdp + i);
          ApplicationContainer app = udpServer.Install (apNode.Get (0));
          app.Start (Seconds (0.0));
          app.Stop (Seconds (simTime + 1.0));
          serverApps.Add (app);

          UdpClientHelper udpClient (apIf.GetAddress (0), portUdp + i);
          udpClient.SetAttribute ("MaxPackets", UintegerValue (0));
          udpClient.SetAttribute ("Interval", TimeValue (Seconds (double (payloadSize * 8.0) / std::stod (offeredLoad.substr(0,offeredLoad.find("M")))*1e6))); // fix for corresponding rate
          udpClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          ApplicationContainer app2 = udpClient.Install (staBNodes.GetN () + staGNodes.GetN () + i < staBNodes.GetN () ?
                                                         staBNodes.Get (i)
                                                         : staGNodes.GetN () + i - staBNodes.GetN () < staGNodes.GetN () ?
                                                           staGNodes.Get (i - staBNodes.GetN ())
                                                           : staNNodes.Get (i - staBNodes.GetN () - staGNodes.GetN ())
                                                        );
          app2.Start (Seconds (1.0));
          app2.Stop (Seconds (simTime + 1.0));
          clientApps.Add (app2);
        }
      else
        {
          // TCP application: BulkSend
          uint16_t thisPort = portTcp + i;
          Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), thisPort));
          PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkLocalAddress);
          ApplicationContainer app = sink.Install (apNode.Get (0));
          app.Start (Seconds (0.0));
          app.Stop (Seconds (simTime + 1.0));
          serverApps.Add (app);

          BulkSendHelper bulkSender ("ns3::TcpSocketFactory",InetSocketAddress (apIf.GetAddress (0), thisPort));
          bulkSender.SetAttribute ("MaxBytes", UintegerValue (0));
          bulkSender.SetAttribute ("SendSize", UintegerValue (payloadSize));
          ApplicationContainer app2 = bulkSender.Install (staBNodes.GetN () + staGNodes.GetN () + i < staBNodes.GetN () ?
                                                         staBNodes.Get (i)
                                                         : staGNodes.GetN () + i - staBNodes.GetN () < staGNodes.GetN () ?
                                                           staGNodes.Get (i - staBNodes.GetN ())
                                                           : staNNodes.Get (i - staBNodes.GetN () - staGNodes.GetN ())
                                                        );
          app2.Start (Seconds (1.0));
          app2.Stop (Seconds (simTime + 1.0));
          clientApps.Add (app2);
        }
    }

  // ----- FlowMonitor -----
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // ---- Run simulation ----
  Simulator::Stop (Seconds (simTime + 2.0));
  Simulator::Run ();

  // ---- Results: print throughput for each flow ----
  std::cout << "\n========= Configuration =========" << std::endl;
  std::cout << "11b STA: " << nB << ", 11g (ERP, non-HT) STA: " << nG << ", 11n (HT) STA: " << nN << std::endl;
  std::cout << "ERP Protection: " << (useErpProtection ? "enabled" : "disabled") << std::endl;
  std::cout << "Short Slot: " << (useShortSlot ? "enabled" : "disabled") << std::endl;
  std::cout << "Short Preamble/PPDU: " << (useShortPreamble ? "enabled" : "disabled") << std::endl;
  std::cout << "Short GI: " << (useShortGuardInterval ? "enabled" : "disabled") << std::endl;
  std::cout << "Short PPDU: " << (useShortPpdu ? "enabled" : "disabled") << std::endl;
  std::cout << "Payload: " << payloadSize << " bytes\t Protocol: " << (useTcp ? "TCP" : "UDP") << std::endl;
  std::cout << "=================================" << std::endl << std::endl;
  PrintThroughput (monitor);

  Simulator::Destroy ();
  return 0;
}