#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <set>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi7McsChannelWidthGiThroughput");

struct SimParams
{
  std::vector<uint8_t> mcsVec;
  std::vector<uint16_t> channelWidths;
  std::vector<std::string> guardIntervals; // "HE-GI-0.8us", "HE-GI-1.6us", "HE-GI-3.2us"
  std::string frequency; // "2.4", "5", or "6"
  bool uplinkOfdma;
  bool bspr;
  uint32_t nSta;
  std::string trafficType; // "TCP" or "UDP"
  uint32_t payloadSize;
  uint32_t mpduBufferSize;
  double simTime; // in seconds
  std::string auxiliaryPhySettings; // optional, implement as a string to parse additional wifi phy settings if necessary
};

// Wi-Fi 7 guard interval and modulation options for ns-3 802.11be
struct GiTable
{
  std::string config; // e.g., "HE-GI-0.8us"
  Time value;
};

static std::vector<GiTable> GetGiTable(const std::vector<std::string>& giNames)
{
  std::vector<GiTable> ret;
  for (const auto& n : giNames)
    {
      if (n == "HE-GI-0.8us") ret.push_back ({n, MicroSeconds (800)});
      else if (n == "HE-GI-1.6us") ret.push_back ({n, MicroSeconds (1600)});
      else if (n == "HE-GI-3.2us") ret.push_back ({n, MicroSeconds (3200)});
      else ret.push_back ({n, MicroSeconds (800)});
    }
  return ret;
}

struct ThroughputResult
{
  uint8_t mcs;
  uint16_t channelWidth;
  std::string gi;
  double throughputMbps;
  bool valid;
  std::string errorMsg;
};

void PrintResultsTable(const std::vector<ThroughputResult>& results)
{
  std::cout << std::left << std::setw(5) << "MCS"
            << std::setw(15) << "ChannelWidth"
            << std::setw(12) << "GI"
            << std::setw(15) << "Throughput(Mbps)"
            << std::endl;
  for (const auto& r : results)
    {
      std::cout << std::setw(5) << unsigned(r.mcs)
                << std::setw(15) << r.channelWidth
                << std::setw(12) << r.gi
                << std::setw(15) << std::fixed << std::setprecision(2)
                << r.throughputMbps;
      if (!r.valid)
        std::cout << " ERROR: " << r.errorMsg;
      std::cout << std::endl;
    }
}

double GetMinimumExpectedThroughput(uint8_t mcs, uint16_t channelWidth, const std::string& gi, uint32_t nSta)
{
  // Conservative: expected per station throughput >= PHY_RATE * (efficiency factor)
  // For Wi-Fi 7, let's use nominal PHY rates, efficiency=0.5, and only divide if more than one station
  // Not precise; for real calculation, fetch data from a standard table.
  double base_phy_rates[12][4] = {
    // ChannelWidth = {20, 40, 80, 160} MHz, all in Mbps for 6 GHz band, MCS 0-11
    { 81.0,    162.9,    325.8,   650.0 },   // MCS 0
    {162.9,    325.8,    650.0,  1300.0 },   // MCS 1
    {243.9,    487.8,    975.0,  1950.0 },   // MCS 2
    {325.8,    650.0,   1300.1,  2600.0 },   // MCS 3
    {487.8,    975.0,   1950.0,  3900.0 },   // MCS 4
    {650.0,   1300.0,   2600.0,  5200.0 },   // MCS 5
    {780.2,   1560.0,   3120.0,  6240.0 },   // MCS 6
    {866.7,   1733.0,   3466.9,  6933.0 },   // MCS 7
    {975.0,   1950.0,   3900.0,  7800.0 },   // MCS 8
    {1083.0,  2166.0,   4332.0,  8662.0 },   // MCS 9
    {1170.0,  2340.0,   4680.0,  9360.0 },   // MCS 10
    {1300.0,  2600.0,   5200.0, 10400.0 },   // MCS 11
  };
  uint8_t idx_mcs = std::min(uint8_t(11), mcs);
  uint8_t chIdx = 0;
  if (channelWidth == 20) chIdx = 0;
  else if (channelWidth == 40) chIdx = 1;
  else if (channelWidth == 80) chIdx = 2;
  else if (channelWidth == 160) chIdx = 3;
  else chIdx = 2;
  double phyRate = base_phy_rates[idx_mcs][chIdx];
  double giFactor = 1.0;
  if (gi == "HE-GI-1.6us") giFactor = 0.95; // traded off due to longer GI
  if (gi == "HE-GI-3.2us") giFactor = 0.92;
  // Assume user payload efficiency: TCP ~0.5, UDP ~0.7
  double efficiency = 0.5;
  double perStation = (phyRate * giFactor * efficiency) / std::max(1u, nSta);
  return perStation * 0.8; // minimum: 80% of best guess
}
double GetMaximumExpectedThroughput(uint8_t mcs, uint16_t channelWidth, const std::string& gi, uint32_t nSta)
{
  return GetMinimumExpectedThroughput(mcs, channelWidth, gi, nSta) * 1.5;
}

// Parse freq string into ns-3 Channel/Frequency
uint16_t GetFreqChannelNum (const std::string& freq)
{
  if (freq == "2.4") return 1; // channel 1
  if (freq == "5") return 36; // ch36/5180 MHz
  if (freq == "6") return 5; // ch5/5935 MHz (6 GHz initial)
  return 1;
}
uint32_t GetCenterFrequencyMHz (const std::string& freq)
{
  if (freq == "2.4") return 2412;
  if (freq == "5") return 5180;
  if (freq == "6") return 5935;
  return 2412;
}

// Traffic generator
void InstallServerClientApps(std::string type, NodeContainer &apNode, NodeContainer &staNodes, uint32_t payloadSize, double totalSimulationTime, ApplicationContainer &serverApps, ApplicationContainer &clientApps)
{
  if (type == "TCP")
    {
      // TCP Sink at AP
      uint16_t port = 50000;
      Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
      serverApps = sinkHelper.Install(apNode);

      // TCP app on each STA to AP
      for (uint32_t i = 0; i < staNodes.GetN (); ++i)
        {
          OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
          clientHelper.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Value=1]"));
          clientHelper.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Value=0]"));
          clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          // Destination address will be set after IP assigned
          clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
          clientApps.Add (clientHelper.Install(staNodes.Get(i)));
        }
    }
  else // UDP
    {
      uint16_t port = 50001;
      UdpServerHelper serverHelper(port);
      serverApps = serverHelper.Install(apNode);

      for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        {
          UdpClientHelper clientHelper (apNode.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port);
          clientHelper.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
          clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));
          clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          clientApps.Add (clientHelper.Install (staNodes.Get(i)));
        }
    }
}

struct ResultCollector
{
  uint64_t rxBytes;
  void RxCallback(Ptr<const Packet> pkt, const Address &src)
  {
    rxBytes += pkt->GetSize ();
  }
};

int main (int argc, char *argv[])
{
  SimParams sp;

  std::string mcsList = "0,1,2,3,4,5,6,7,8,9,10,11";
  std::string channelWidths = "20,40,80,160";
  std::string giList = "HE-GI-0.8us,HE-GI-1.6us,HE-GI-3.2us";
  std::string freq = "6";
  bool uplinkOfdma = false;
  bool bspr = false;
  uint32_t nSta = 4;
  std::string trafficType = "UDP";
  uint32_t payloadSize = 1400;
  uint32_t mpduBufferSize = 64; // not used directly here
  std::string auxPhySettings = "";
  double simTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("mcs", "Comma-separated MCS values", mcsList);
  cmd.AddValue ("channelWidths", "Comma-separated channel widths (MHz)", channelWidths);
  cmd.AddValue ("guardIntervals", "Comma-separated GI: HE-GI-0.8us,HE-GI-1.6us,HE-GI-3.2us", giList);
  cmd.AddValue ("frequency", "Operating band: 2.4, 5, or 6", freq);
  cmd.AddValue ("uplinkOfdma", "Enable UL OFDMA", uplinkOfdma);
  cmd.AddValue ("bspr", "Enable BSRP", bspr);
  cmd.AddValue ("nSta", "Number of Stations", nSta);
  cmd.AddValue ("trafficType", "TCP or UDP", trafficType);
  cmd.AddValue ("payloadSize", "Bytes per packet", payloadSize);
  cmd.AddValue ("mpduBufferSize", "MPDU buffer size (not directly used here)", mpduBufferSize);
  cmd.AddValue ("auxPhySettings", "Auxiliary PHY settings string", auxPhySettings);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.Parse (argc, argv);

  std::vector<uint8_t> mcsVec;
  std::stringstream mscl (mcsList);
  std::string tmp;
  while (std::getline(mscl, tmp, ',')) mcsVec.push_back (uint8_t (std::stoi(tmp)));

  std::vector<uint16_t> chwidthVec;
  std::stringstream chw (channelWidths);
  while (std::getline(chw, tmp, ',')) chwidthVec.push_back (uint16_t (std::stoi(tmp)));

  std::vector<std::string> giVec;
  std::stringstream gil (giList);
  while (std::getline(gil, tmp, ',')) giVec.push_back (tmp);

  sp.mcsVec = mcsVec;
  sp.channelWidths = chwidthVec;
  sp.guardIntervals = giVec;
  sp.frequency = freq;
  sp.uplinkOfdma = uplinkOfdma;
  sp.bspr = bspr;
  sp.nSta = nSta;
  sp.trafficType = trafficType;
  sp.payloadSize = payloadSize;
  sp.mpduBufferSize = mpduBufferSize;
  sp.simTime = simTime;
  sp.auxiliaryPhySettings = auxPhySettings;

  std::vector<ThroughputResult> results;

  std::set<std::tuple<uint8_t,uint16_t,std::string>> testedConfigs;

  auto giTable = GetGiTable(sp.guardIntervals);

  for (auto mcs : sp.mcsVec)
    for (auto cw : sp.channelWidths)
      for (const auto& giEntry : giTable)
        {
          if (testedConfigs.count({mcs,cw,giEntry.config})) continue;
          testedConfigs.insert({mcs,cw,giEntry.config});

          // Wi-Fi setup
          NodeContainer wifiStaNodes, wifiApNode;
          wifiStaNodes.Create (sp.nSta);
          wifiApNode.Create (1);

          YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
          YansWifiPhyHelper phy;
          phy.SetChannel (channel.Create());

          // 11be (Wi-Fi 7)
          WifiHelper wifi;
          wifi.SetStandard (WIFI_STANDARD_80211be);

          // OFDMA, BSRP settings
          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HeMcs"+std::to_string(mcs)),
                                       "ControlMode", StringValue("HeMcs0"));

          WifiMacHelper mac;
          Ssid ssid = Ssid ("wifi7-ssid");

          phy.Set ("ChannelWidth", UintegerValue (cw));
          phy.Set ("Frequency", UintegerValue (GetCenterFrequencyMHz(sp.frequency)));
          phy.Set ("GuardInterval", TimeValue (giEntry.value));

          if (!sp.auxiliaryPhySettings.empty())
            { /* Apply extra PHY settings here if required */
            }
          // Set up AP and STA MAC/PHY
          mac.SetType ("ns3::StaWifiMac","Ssid", SsidValue (ssid),"ActiveProbing", BooleanValue (false));
          NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNodes);

          mac.SetType ("ns3::ApWifiMac","Ssid", SsidValue (ssid));
          NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

          // Internet stack/IP
          InternetStackHelper stack;
          stack.Install (wifiApNode);
          stack.Install (wifiStaNodes);

          Ipv4AddressHelper address;
          address.SetBase ("10.1.1.0", "255.255.255.0");
          Ipv4InterfaceContainer staIf = address.Assign (staDevice);
          Ipv4InterfaceContainer apIf = address.Assign (apDevice);

          // Mobility (static)
          MobilityHelper mobility;
          mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
          mobility.Install (wifiStaNodes);
          mobility.Install (wifiApNode);

          // Applications
          ApplicationContainer serverApps, clientApps;
          if (sp.trafficType == "TCP") // TCP Sink at AP, TCP sources at each STA
            {
              uint16_t port = 50000;
              Address apAddr (InetSocketAddress (apIf.GetAddress (0), port));
              PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
              serverApps = sinkHelper.Install (wifiApNode);

              OnOffHelper onoff ("ns3::TcpSocketFactory", apAddr);
              onoff.SetAttribute ("PacketSize", UintegerValue (sp.payloadSize));
              onoff.SetAttribute ("DataRate", StringValue ("2Gbps"));
              onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Value=1.0]"));
              onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Value=0.0]"));
              for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
                {
                  clientApps.Add (onoff.Install (wifiStaNodes.Get (i)));
                }
            }
          else // UDP traffic: UDPServer at AP
            {
              uint16_t port = 50001;
              UdpServerHelper serverHelper(port);
              serverApps = serverHelper.Install(wifiApNode);

              for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
                {
                  UdpClientHelper clientHelper (apIf.GetAddress (0), port);
                  clientHelper.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
                  clientHelper.SetAttribute ("Interval", TimeValue (MicroSeconds (100)));
                  clientHelper.SetAttribute ("PacketSize", UintegerValue (sp.payloadSize));
                  clientApps.Add (clientHelper.Install (wifiStaNodes.Get(i)));
                }
            }

          serverApps.Start (Seconds (0.0));
          clientApps.Start (Seconds (0.1));
          clientApps.Stop  (Seconds (sp.simTime));
          serverApps.Stop  (Seconds (sp.simTime + 0.1));

          // Throughput measurement
          Ptr<PacketSink> pktSink;
          Ptr<UdpServer> udpServer;
          if (sp.trafficType == "TCP")
            pktSink = DynamicCast<PacketSink> (serverApps.Get (0));
          else
            udpServer = DynamicCast<UdpServer> (serverApps.Get (0));

          Simulator::Stop (Seconds (sp.simTime+0.2));

          Simulator::Run ();

          double rxBytes = 0.0;
          if (sp.trafficType == "TCP")
            rxBytes = pktSink->GetTotalRx ();
          else
            rxBytes = udpServer->GetReceived () * sp.payloadSize;

          double throughputMbps = (rxBytes * 8.0) / (sp.simTime * 1e6);

          double minExp = GetMinimumExpectedThroughput(mcs, cw, giEntry.config, sp.nSta);
          double maxExp = GetMaximumExpectedThroughput(mcs, cw, giEntry.config, sp.nSta);

          bool valid = true;
          std::string err = "";

          if (throughputMbps < minExp)
            {
              valid = false;
              std::ostringstream oss;
              oss << "Throughput (" << throughputMbps << " Mbps) less than minimum expected (" << minExp << " Mbps).";
              err = oss.str();
            }
          if (throughputMbps > maxExp)
            {
              valid = false;
              std::ostringstream oss;
              oss << "Throughput (" << throughputMbps << " Mbps) greater than maximum expected (" << maxExp << " Mbps).";
              err = oss.str();
            }
          results.push_back (
              {mcs, cw, giEntry.config, throughputMbps, valid, err});

          Simulator::Destroy ();
          if (!valid)
            {
              PrintResultsTable (results);
              NS_FATAL_ERROR("ERROR: Out of bounds throughput for MCS "
                              << unsigned(mcs)
                              << ", CW=" << cw << ", GI=" << giEntry.config
                              << ": "<< err);
              return 1;
            }
        }
  PrintResultsTable (results);

  return 0;
}