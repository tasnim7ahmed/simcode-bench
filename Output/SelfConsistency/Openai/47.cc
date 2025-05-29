/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Spatial Reuse Simulation with 802.11ax (OBSS-PD)
 * Two BSS: (AP1, STA1) and (AP2, STA2), configurable by command line.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpatialReuseSimulation");

struct BssData
{
  Ptr<Node> ap;
  Ptr<Node> sta;
  double throughput;
};

static void
LogSpatialReuseEvent (std::string context, WifiRemoteStationManager::Event event, uint32_t staId, double rssi, double threshold, const std::string& file)
{
  std::ofstream os;
  os.open (file, std::ios_base::app);
  os << Simulator::Now ().GetSeconds ()
     << "s: STA" << staId
     << " event=" << event
     << " RSSI=" << rssi
     << " threshold=" << threshold
     << std::endl;
  os.close ();
}

static void
RxCallback (Ptr<const Packet> packet, const Address& addr,
            std::map<Ptr<Node>, uint32_t>* rxMap, Ptr<Node> ap)
{
  (*rxMap)[ap] += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  // Default parameters
  double d1 = 5.0;     // STA1 <-> AP1
  double d2 = 5.0;     // STA2 <-> AP2
  double d3 = 20.0;    // AP1 <-> AP2 (distance between APs)
  double txPower = 18; // dBm
  double ccaEd = -62;  // dBm
  double obssPd = -72; // dBm
  bool enableObssPd = true;
  std::string channelWidth = "20MHz";
  double simTime = 10.0; // seconds
  Time pktInterval = MilliSeconds (1);
  uint32_t pktSize = 1024;
  std::string logFile1 = "bss1_spatial_reuse.log";
  std::string logFile2 = "bss2_spatial_reuse.log";

  CommandLine cmd;
  cmd.AddValue ("d1", "Distance between STA1 and AP1 [m]", d1);
  cmd.AddValue ("d2", "Distance between STA2 and AP2 [m]", d2);
  cmd.AddValue ("d3", "Distance between AP1 and AP2 [m]", d3);
  cmd.AddValue ("txPower", "Transmit power in dBm", txPower);
  cmd.AddValue ("ccaEd", "CCA-ED threshold in dBm", ccaEd);
  cmd.AddValue ("obssPd", "OBSS PD threshold in dBm", obssPd);
  cmd.AddValue ("enableObssPd", "Enable OBSS-PD spatial reuse (true/false)", enableObssPd);
  cmd.AddValue ("channelWidth", "Channel width: 20MHz, 40MHz, 80MHz", channelWidth);
  cmd.AddValue ("simTime", "Simulation time [s]", simTime);
  cmd.AddValue ("pktInterval", "Inter-packet interval (ms)", pktInterval);
  cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
  cmd.AddValue ("logFile1", "Log file for BSS1", logFile1);
  cmd.AddValue ("logFile2", "Log file for BSS2", logFile2);
  cmd.Parse (argc, argv);

  // Prepare log files
  std::ofstream (logFile1, std::ios_base::trunc).close ();
  std::ofstream (logFile2, std::ios_base::trunc).close ();

  // Create nodes
  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (2);

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ax);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (txPower));
  phy.Set ("TxPowerEnd", DoubleValue (txPower));
  phy.Set ("CcaEdThreshold", DoubleValue (ccaEd));

  // Channel width & related settings
  if (channelWidth == "20MHz")
    phy.Set ("ChannelWidth", UintegerValue (20));
  else if (channelWidth == "40MHz")
    phy.Set ("ChannelWidth", UintegerValue (40));
  else if (channelWidth == "80MHz")
    phy.Set ("ChannelWidth", UintegerValue (80));
  else
    {
      NS_ABORT_MSG ("Unsupported channel width " << channelWidth);
    }

  // MAC Helper (one SSID per BSS)
  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("BSS1");
  Ssid ssid2 = Ssid ("BSS2");

  // Install AP devices
  NetDeviceContainer apDevices;
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));
  apDevices.Add (wifi.Install (phy, mac, apNodes.Get (0)));
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));
  apDevices.Add (wifi.Install (phy, mac, apNodes.Get (1)));

  // Install STA devices
  NetDeviceContainer staDevices;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1));
  staDevices.Add (wifi.Install (phy, mac, staNodes.Get (0)));
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2));
  staDevices.Add (wifi.Install (phy, mac, staNodes.Get (1)));

  // Set spatial reuse (OBSS-PD) and thresholds for each STA
  for (uint32_t i = 0; i < 2; ++i)
    {
      Ptr<NetDevice> dev = staDevices.Get (i);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
      Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac> (wifiDev->GetMac ());
      if (enableObssPd)
        {
          StringValue obssPdStr (std::to_string (obssPd) + "dBm");
          staMac->SetAttribute ("ObssPdThreshold", obssPdStr);
        }
      else
        {
          DoubleValue minObssPd (-82);
          staMac->SetAttribute ("ObssPdThreshold", minObssPd);
        }
    }

  // Mobility: place AP1 at (0,0,0), STA1 at (d1,0,0)
  //           AP2 at (d3,0,0), STA2 at (d3+d2,0,0)
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0, 0, 0));                  // AP1
  posAlloc->Add (Vector (d3, 0, 0));                 // AP2
  posAlloc->Add (Vector (d1, 0, 0));                 // STA1
  posAlloc->Add (Vector (d3 + d2, 0, 0));            // STA2

  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  for (uint32_t i = 0; i < 2; ++i)
    mobility.Install (apNodes.Get (i));
  for (uint32_t i = 0; i < 2; ++i)
    mobility.Install (staNodes.Get (i));

  // Internet stack and IPs
  InternetStackHelper stack;
  NodeContainer allNodes;
  allNodes.Add (apNodes);
  allNodes.Add (staNodes);
  stack.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apIfaces;
  apIfaces.Add (ipv4.Assign (apDevices));
  ipv4.NewNetwork ();
  Ipv4InterfaceContainer staIfaces;
  staIfaces.Add (ipv4.Assign (staDevices));

  // UDP Applications: STA1 -> AP1, STA2 -> AP2
  ApplicationContainer serverApps, clientApps;
  for (uint32_t i = 0; i < 2; ++i)
    {
      UdpServerHelper server (9000);
      serverApps.Add (server.Install (apNodes.Get (i)));

      UdpClientHelper client (apIfaces.GetAddress (i), 9000);
      client.SetAttribute ("MaxPackets", UintegerValue (uint32_t (-1)));
      client.SetAttribute ("Interval", TimeValue (pktInterval));
      client.SetAttribute ("PacketSize", UintegerValue (pktSize));
      clientApps.Add (client.Install (staNodes.Get (i)));
    }
  serverApps.Start (Seconds (0.5));
  serverApps.Stop (Seconds (simTime));
  clientApps.Start (Seconds (0.5));
  clientApps.Stop (Seconds (simTime));

  // Throughput counters (per BSS)
  std::map<Ptr<Node>, uint32_t> rxBytes;
  for (uint32_t i = 0; i < 2; ++i)
    {
      rxBytes[apNodes.Get (i)] = 0;
      // connect trace to AP MAC layer for packets received
      Ptr<NetDevice> apDev = apDevices.Get (i);
      Ptr<WifiNetDevice> wifiApDev = DynamicCast<WifiNetDevice> (apDev);
      Ptr<WifiMac> mac = wifiApDev->GetMac ();
      std::ostringstream oss;
      oss << "/NodeList/" << apNodes.Get(i)->GetId() << "/ApplicationList/0/$ns3::UdpServer/Rx";
      Config::ConnectWithoutContext (oss.str (), MakeBoundCallback (&RxCallback, &rxBytes, apNodes.Get (i)));
    }

  // Event logging for OBSS-PD/Spatial Reuse
  Config::Connect ("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Mac/ObssPdReset",
                   MakeBoundCallback (&LogSpatialReuseEvent, 1, obssPd, logFile1));
  Config::Connect ("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Mac/ObssPdReset",
                   MakeBoundCallback (&LogSpatialReuseEvent, 2, obssPd, logFile2));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Calculate throughput
  double bss1Throughput = (rxBytes[apNodes.Get (0)] * 8.0) / (simTime * 1e6); // Mbps
  double bss2Throughput = (rxBytes[apNodes.Get (1)] * 8.0) / (simTime * 1e6); // Mbps

  std::cout << std::fixed << std::setprecision (6);
  std::cout << "Simulation completed.\n";
  std::cout << "Spatial reuse (OBSS-PD) " << (enableObssPd ? "ENABLED" : "DISABLED") << "\n";
  std::cout << "BSS1 (STA1->AP1) throughput: " << bss1Throughput << " Mbps" << std::endl;
  std::cout << "BSS2 (STA2->AP2) throughput: " << bss2Throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}