/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <vector>
#include <limits>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAggregationDemo");

// Structure to hold throughput and TXOP stats
struct NetStats
{
  uint64_t totalRx;
  double tputMbps;
  Time maxTxop;
};

std::vector<NetStats> g_netStats (4);

void RxCallback (uint32_t netIndex, Ptr<const Packet> packet, const Address &)
{
  g_netStats[netIndex].totalRx += packet->GetSize ();
}

void TxopTraceCallback (uint32_t netIndex, Time txop)
{
  if (txop > g_netStats[netIndex].maxTxop)
    {
      g_netStats[netIndex].maxTxop = txop;
    }
}

int main (int argc, char *argv[])
{
  double distance = 10.0; // meters
  bool rtsCts = false;
  double txopLimitUs = 3264; // default TXOP limit in microseconds (assuming BlockAck)

  CommandLine cmd;
  cmd.AddValue ("distance", "Distance between AP and STA in meters", distance);
  cmd.AddValue ("rtsCts", "Enable/disable RTS/CTS", rtsCts);
  cmd.AddValue ("txopLimitUs", "TXOP limit in microseconds", txopLimitUs);
  cmd.Parse (argc, argv);

  // Network params
  const uint32_t numNets = 4;
  std::vector<uint16_t> channels = { 36, 40, 44, 48 };
  // Aggregation settings per STA
  struct AggSetting {
    bool ampdu;
    uint32_t ampduSize;
    bool amsdu;
    uint32_t amsduSize;
  };
  std::vector<AggSetting> aggSettings = {
    {true, 65535, false, 0},       // A: default A-MPDU, 65kB
    {false, 0, false, 0},          // B: aggregation disabled
    {false, 0, true, 7935},        // C: A-MSDU enabled, 8kB (7935 bytes max)
    {true, 32768, true, 4095}      // D: both enabled, A-MPDU 32kB, A-MSDU 4kB
  };

  // Setup AP/STA nodes per network
  std::vector<NodeContainer> apNodes (numNets);
  std::vector<NodeContainer> staNodes (numNets);
  std::vector<NetDeviceContainer> apDevs (numNets);
  std::vector<NetDeviceContainer> staDevs (numNets);
  std::vector<YansWifiPhyHelper> phys (numNets);
  std::vector<YansWifiChannelHelper> channelsHelper (numNets);
  std::vector<WifiHelper> wifiHelpers (numNets);
  std::vector<WifiMacHelper> macHelpers (numNets);
  std::vector<MobilityHelper> mobilityHelpers (numNets);

  InternetStackHelper stack;

  for (uint32_t i = 0; i < numNets; ++i)
    {
      apNodes[i].Create (1);
      staNodes[i].Create (1);

      // PHY and Channel setup (5 GHz, channel 36/40/44/48)
      channelsHelper[i] = YansWifiChannelHelper::Default ();
      phys[i] = YansWifiPhyHelper::Default ();
      Ptr<YansWifiChannel> channel = channelsHelper[i].Create ();
      phys[i].SetChannel (channel);
      phys[i].Set ("ChannelNumber", UintegerValue (channels[i]));
      phys[i].Set ("Frequency", UintegerValue (5180 + (channels[i] - 36) * 5));

      // Setup Wi-Fi 802.11n
      wifiHelpers[i].SetStandard (WIFI_STANDARD_80211n_5GHZ);
      wifiHelpers[i].SetRemoteStationManager
        ("ns3::ConstantRateWifiManager",
         "DataMode", StringValue ("HtMcs7"),
         "ControlMode", StringValue ("HtMcs0"),
         "RtsCtsThreshold", UintegerValue (rtsCts ? 0 : 2347));

      // MAC
      macHelpers[i].SetType ("ns3::ApWifiMac",
                             "Ssid", SsidValue (Ssid ("net" + std::to_string (i))));
      apDevs[i] = wifiHelpers[i].Install (phys[i], macHelpers[i], apNodes[i].Get (0));

      macHelpers[i].SetType ("ns3::StaWifiMac",
                             "Ssid", SsidValue (Ssid ("net" + std::to_string (i))),
                             "ActiveProbing", BooleanValue (false));
      staDevs[i] = wifiHelpers[i].Install (phys[i], macHelpers[i], staNodes[i].Get (0));

      // Aggregation settings
      WifiMacHelper::SetWifiMacType ("ns3::StaWifiMac");
      WifiHelper::SetRemoteStationManager ("ns3::ConstantRateWifiManager");

      // Configure aggregation on each STA device
      Ptr<NetDevice> dev = staDevs[i].Get (0);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
      Ptr<WifiMac> mac = wifiDev->GetMac ();
      if (aggSettings[i].ampdu)
        {
          mac->SetAttribute ("BE_MaxAmpduSize", UintegerValue (aggSettings[i].ampduSize));
          mac->SetAttribute ("BE_BlockAckThreshold", UintegerValue (1)); // enable BA
        }
      else
        {
          mac->SetAttribute ("BE_MaxAmpduSize", UintegerValue (0));
          mac->SetAttribute ("BE_BlockAckThreshold", UintegerValue (65535)); // disables BA
        }
      if (aggSettings[i].amsdu)
        {
          mac->SetAttribute ("BE_MaxAmsduSize", UintegerValue (aggSettings[i].amsduSize));
          mac->SetAttribute ("BE_QosAmsduSupport", BooleanValue (true));
        }
      else
        {
          mac->SetAttribute ("BE_MaxAmsduSize", UintegerValue (0));
          mac->SetAttribute ("BE_QosAmsduSupport", BooleanValue (false));
        }

      // Set TXOP duration if specified (on both AP and STA)
      Ptr<NetDevice> apDev = apDevs[i].Get (0);
      Ptr<WifiNetDevice> apWifiDev = DynamicCast<WifiNetDevice> (apDev);
      Ptr<WifiMac> apMac = apWifiDev->GetMac ();

      HcfParameterSet hcfPs;
      hcfPs.qosInfo = 0;
      // Only setting TXOP limit for access category BE (index 0)
      HtConfiguration htConf;
      apMac->SetAttribute ("BE_EdcaTxopN", UintegerValue ((uint32_t) txopLimitUs )); // microseconds
      mac->SetAttribute ("BE_EdcaTxopN", UintegerValue ((uint32_t) txopLimitUs ));

      // Mobility
      mobilityHelpers[i].SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobilityHelpers[i].Install (apNodes[i]);
      mobilityHelpers[i].Install (staNodes[i]);
      Ptr<MobilityModel> apMob = apNodes[i].Get (0)->GetObject<MobilityModel> ();
      Ptr<MobilityModel> staMob = staNodes[i].Get (0)->GetObject<MobilityModel> ();
      apMob->SetPosition (Vector (0.0, 0.0, 0.0));
      staMob->SetPosition (Vector (distance, 0.0, 0.0));

      // IP stack
      stack.Install (apNodes[i]);
      stack.Install (staNodes[i]);
    }

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> apIfs (numNets);
  std::vector<Ipv4InterfaceContainer> staIfs (numNets);

  for (uint32_t i = 0; i < numNets; ++i)
    {
      std::string net = "10.1." + std::to_string (i+1) + ".0";
      address.SetBase (net.c_str (), "255.255.255.0");
      apIfs[i] = address.Assign (apDevs[i]);
      staIfs[i] = address.Assign (staDevs[i]);
    }

  // Applications: FTP-like flow from STA to AP
  uint16_t port = 50000;
  double appStart = 1.0;
  double appStop = 10.0;

  for (uint32_t i = 0; i < numNets; ++i)
    {
      // Install receiver on AP
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp = sinkHelper.Install (apNodes[i].Get (0));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (appStop + 1.0));
      sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxCallback, i));

      // Install sender on STA
      OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (apIfs[i].GetAddress (0), port));
      onoff.SetAttribute ("DataRate", StringValue ("200Mbps"));
      onoff.SetAttribute ("PacketSize", UintegerValue (4096));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (appStart)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (appStop)));
      // No need to keep handle
      onoff.Install (staNodes[i].Get (0));
    }

  // TXOP tracing (uses EdcaTxopN::TxopTrace at the AP's MAC)
  for (uint32_t i = 0; i < numNets; ++i)
    {
      Ptr<NetDevice> apDev = apDevs[i].Get (0);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (apDev);
      Ptr<WifiMac> mac = wifiDev->GetMac ();
      // EDCA Txop for BE (AC_BE)
      Ptr<EdcaTxopN> edca = mac->GetEdcaTxopN (AC_BE);
      edca->TraceConnectWithoutContext ("TxopTrace", MakeBoundCallback (&TxopTraceCallback, i));
      g_netStats[i].totalRx = 0;
      g_netStats[i].tputMbps = 0.0;
      g_netStats[i].maxTxop = Seconds (0);
    }

  Simulator::Stop (Seconds (appStop + 1.0));
  Simulator::Run ();

  // Results
  std::cout << "\n==== Aggregation/CQ stats across 4 networks ====\n";
  for (uint32_t i = 0; i < numNets; ++i)
    {
      double duration = appStop - appStart;
      g_netStats[i].tputMbps = (g_netStats[i].totalRx * 8.0) / (duration * 1e6);
      std::cout << "Net " << i + 1 << " (chan " << channels[i] << "):\n";
      if (i == 0)
        std::cout << " config: STA A - default A-MPDU 65kB\n";
      else if (i == 1)
        std::cout << " config: STA B - aggregation DISABLED\n";
      else if (i == 2)
        std::cout << " config: STA C - A-MSDU 8kB only\n";
      else if (i == 3)
        std::cout << " config: STA D - A-MPDU 32kB, A-MSDU 4kB\n";
      std::cout << "  Throughput: " << g_netStats[i].tputMbps << " Mbps\n";
      std::cout << "  Max TXOP duration: "
                << g_netStats[i].maxTxop.GetMicroSeconds () << " us\n";
    }

  Simulator::Destroy ();
  return 0;
}