#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// Custom structure to hold Wi-Fi standard and phy band
struct WifiModeInfo
{
  WifiStandard standard;
  std::string phyBand;
};

// Function to parse Wi-Fi version string and return standard and band
WifiModeInfo
ParseWifiVersion (const std::string &version)
{
  if (version == "80211a")
    {
      return {WIFI_STANDARD_80211a, "5GHz"};
    }
  else if (version == "80211b")
    {
      return {WIFI_STANDARD_80211b, "2.4GHz"};
    }
  else if (version == "80211g")
    {
      return {WIFI_STANDARD_80211g, "2.4GHz"};
    }
  else if (version == "80211n-2.4GHz")
    {
      return {WIFI_STANDARD_80211n, "2.4GHz"};
    }
  else if (version == "80211n-5GHz")
    {
      return {WIFI_STANDARD_80211n, "5GHz"};
    }
  else if (version == "80211ac")
    {
      return {WIFI_STANDARD_80211ac, "5GHz"};
    }
  else if (version == "80211ax-2.4GHz")
    {
      return {WIFI_STANDARD_80211ax, "2.4GHz"};
    }
  else if (version == "80211ax-5GHz")
    {
      return {WIFI_STANDARD_80211ax, "5GHz"};
    }
  else
    {
      // Default to 802.11g 2.4GHz
      return {WIFI_STANDARD_80211g, "2.4GHz"};
    }
}

// Throughput calculation helper
static void
CalculateThroughput (Ptr<PacketSink> sink, uint64_t &lastTotalRx, Time &lastTime)
{
  Time now = Simulator::Now();
  double throughput = (sink->GetTotalRx() - lastTotalRx) * 8.0 / (now.GetSeconds() - lastTime.GetSeconds()) / 1e6; // Mbps
  std::cout << now.GetSeconds () << "s: Throughput: " << throughput << " Mbps" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  lastTime = now;
}

int
main (int argc, char *argv[])
{
  std::string apWifiVersion = "80211ac";
  std::string staWifiVersion = "80211g";
  std::string apRateManager = "ns3::AarfWifiManager";
  std::string staRateManager = "ns3::MinstrelWifiManager";
  std::string trafficDirection = "sta_to_ap"; // "ap_to_sta", "sta_to_ap", "bi"
  double simulationTime = 10.0;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("apWifiVersion", "Wi-Fi version for AP (e.g., 80211ac, 80211n-2.4GHz)", apWifiVersion);
  cmd.AddValue ("staWifiVersion", "Wi-Fi version for STA (e.g., 80211g, 80211a)", staWifiVersion);
  cmd.AddValue ("apRateManager", "Rate control algorithm for AP", apRateManager);
  cmd.AddValue ("staRateManager", "Rate control algorithm for STA", staRateManager);
  cmd.AddValue ("trafficDirection", "Traffic direction: sta_to_ap, ap_to_sta, bi", trafficDirection);
  cmd.AddValue ("simulationTime", "Simulation duration (seconds)", simulationTime);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  // Retrieve Wi-Fi settings
  WifiModeInfo apMode = ParseWifiVersion (apWifiVersion);
  WifiModeInfo staMode = ParseWifiVersion (staWifiVersion);

  // Create nodes
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // Set up Wi-Fi device and PHY
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phySta = YansWifiPhyHelper::Default (), phyAp = YansWifiPhyHelper::Default ();
  phySta.SetChannel (channel.Create ());
  phyAp.SetChannel (channel.Create ());

  // Set operating frequency band for PHYs
  if (staMode.phyBand == "5GHz")
    {
      phySta.Set ("Frequency", UintegerValue (5180));
    }
  else
    {
      phySta.Set ("Frequency", UintegerValue (2412));
    }

  if (apMode.phyBand == "5GHz")
    {
      phyAp.Set ("Frequency", UintegerValue (5180));
    }
  else
    {
      phyAp.Set ("Frequency", UintegerValue (2412));
    }

  // Configure Wi-Fi setups for AP and STA
  WifiHelper wifiSta = WifiHelper::Default (), wifiAp = WifiHelper::Default ();
  wifiSta.SetStandard (staMode.standard);
  wifiAp.SetStandard (apMode.standard);

  wifiSta.SetRemoteStationManager (staRateManager);
  wifiAp.SetRemoteStationManager (apRateManager);

  // MAC and device installation
  Ssid ssid = Ssid ("ns3-compat-net");
  WifiMacHelper macSta, macAp;

  macSta.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  macAp.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));

  // Install
  NetDeviceContainer staDevice, apDevice;
  staDevice = wifiSta.Install (phySta, macSta, wifiStaNode);
  apDevice = wifiAp.Install (phyAp, macAp, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel> ();
  apMobility->SetPosition (Vector (0.0, 0.0, 0.0));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-10, 10, -10, 10)));
  mobility.Install (wifiStaNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface, staInterface;
  apInterface = address.Assign (apDevice);
  staInterface = address.Assign (staDevice);

  // Install applications
  uint16_t port = 9;

  ApplicationContainer serverApps, clientApps;
  // AP sends, STA receives
  if (trafficDirection == "ap_to_sta" || trafficDirection == "bi")
    {
      UdpServerHelper server (port);
      serverApps.Add (server.Install (wifiStaNode.Get (0)));

      UdpClientHelper client (staInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005))); // ~2Mbps @ 1400 bytes
      client.SetAttribute ("PacketSize", UintegerValue (1400));

      clientApps.Add (client.Install (wifiApNode.Get (0)));
    }
  // STA sends, AP receives
  if (trafficDirection == "sta_to_ap" || trafficDirection == "bi")
    {
      UdpServerHelper server (port+1);
      serverApps.Add (server.Install (wifiApNode.Get (0)));

      UdpClientHelper client (apInterface.GetAddress (0), port+1);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
      client.SetAttribute ("PacketSize", UintegerValue (1400));

      clientApps.Add (client.Install (wifiStaNode.Get (0)));
    }

  serverApps.Start (Seconds (1.0));
  clientApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime + 1.0));
  clientApps.Stop (Seconds (simulationTime + 1.0));

  // Populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Throughput monitor
  Ptr<PacketSink> sink;
  if (trafficDirection == "sta_to_ap")
    {
      sink = DynamicCast<PacketSink> (serverApps.Get (1));
    }
  else if (trafficDirection == "ap_to_sta")
    {
      sink = DynamicCast<PacketSink> (serverApps.Get (0));
    }
  else // "bi" (take AP sink for reporting)
    {
      sink = DynamicCast<PacketSink> (serverApps.Get (1));
    }

  uint64_t lastTotalRx = 0;
  Time lastTime = Seconds (1.0);

  Simulator::Schedule (Seconds (1.1), [&] {
    Simulator::Schedule (Seconds (1.0), [&] {
      CalculateThroughput(sink, lastTotalRx, lastTime);
      Simulator::Schedule (Seconds (1.0), [&] {
        CalculateThroughput(sink, lastTotalRx, lastTime);
      });
    });
  });

  Simulator::Stop (Seconds (simulationTime + 2.0));
  Simulator::Run ();
  Simulator::Destroy ();

  std::cout << "Simulation complete. Total bytes received by sink: " << sink->GetTotalRx () << std::endl;

  return 0;
}