#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

using namespace ns3;

struct SimParams
{
  std::vector<uint8_t> mcs {0,1,2,3,4,5,6,7,8,9,10,11,12};
  std::vector<uint16_t> channelWidths {20, 40, 80, 160};
  std::vector<std::string> gi {"short", "long"};
  std::vector<double> operFrequencies {2.4, 5, 6};
  bool useUplinkOFDMA {false};
  bool useBSRP {false};
  uint32_t nSta {2};
  std::string trafficType {"UDP"};
  uint32_t payloadSize {1200};
  uint32_t mpduBufferSize {2};
  Time simTime {Seconds (10)};
  std::string phyMode {"11be"};
};

struct SimResult
{
  uint8_t mcs;
  uint16_t chWidth;
  std::string gi;
  double throughputMbps;
  bool error;
};

static std::string GetFreqChannel (double ghz)
{
  if (ghz == 2.4) return "2412";
  if (ghz == 5)   return "5180";
  if (ghz == 6)   return "5955";
  return "2412";
}

static std::string ToGiString (const std::string& gi)
{
  return gi == "short" ? "HeGuardInterval400ns" : "HeGuardInterval800ns";
}

static double ExpectedThroughput (uint8_t mcs, uint16_t width, std::string gi, uint32_t nSta, std::string traffic, double frequency)
{
  // Estimate - very rough (Wi-Fi 7 - this is simplified, actual values depend on details)
  static const double base[] = {8.6, 17.2, 25.8, 34.4, 51.6, 68.8, 77.4, 86, 103.2, 114.7, 129, 143.4, 154.2}; // Mbps at 20MHz, short GI
  double mult = width / 20.0;
  double val = base[std::min((size_t)mcs, sizeof(base)/sizeof(base[0])-1)] * mult;
  if (gi == "long") val *= 0.9;
  if (frequency == 2.4) val *= 0.85;
  if (traffic == "TCP") val *= 0.94;
  // Assume equal sharing
  val /= nSta;
  return val;
}

static double Tolerance () { return 0.18; } // 18% error margin

void PrintResultsTable (const std::vector<SimResult>& results)
{
  std::cout << std::setw(4) << "MCS" << " | "
            << std::setw(10) << "Ch_Width" << " | "
            << std::setw(7) << "GI" << " | "
            << std::setw(14) << "Throughput [Mbps]" << std::endl;
  std::cout << "---------------------------------------------" << std::endl;
  for (const auto& r : results)
  {
    std::cout << std::setw(4) << int(r.mcs) << " | "
              << std::setw(10) << r.chWidth << " | "
              << std::setw(7) << r.gi << " | "
              << std::setw(14) << std::fixed << std::setprecision(2) << r.throughputMbps
              << (r.error ? "  ERROR" : "")
              << std::endl;
  }
}

int main (int argc, char *argv[])
{
  // Parameters
  SimParams params;

  CommandLine cmd;
  cmd.AddValue ("mcs", "Comma separated MCS values", params.mcs);
  cmd.AddValue ("channelWidths", "Comma separated channel widths", params.channelWidths);
  cmd.AddValue ("gi", "Comma separated guard intervals (short|long)", params.gi);
  cmd.AddValue ("freq", "Comma separated operating frequencies (2.4,5,6)", params.operFrequencies);
  cmd.AddValue ("uplinkOFDMA", "Enable uplink OFDMA", params.useUplinkOFDMA);
  cmd.AddValue ("bsrp", "Enable BSRP", params.useBSRP);
  cmd.AddValue ("nSta", "Number of stations", params.nSta);
  cmd.AddValue ("trafficType", "TCP or UDP", params.trafficType);
  cmd.AddValue ("payloadSize", "Payload size (bytes)", params.payloadSize);
  cmd.AddValue ("mpduBufferSize", "MPDU buffer size", params.mpduBufferSize);
  cmd.AddValue ("simTime", "Simulation time (s)", params.simTime);
  cmd.Parse (argc, argv);

  std::vector<SimResult> results;

  for (double freq : params.operFrequencies)
  {
    uint16_t channelNumber = std::stoi (GetFreqChannel (freq));
    for (uint8_t mcs : params.mcs)
    {
      for (uint16_t width : params.channelWidths)
      {
        for (const std::string& gi : params.gi)
        {
          NodeContainer wifiStaNodes, wifiApNode;
          wifiStaNodes.Create (params.nSta);
          wifiApNode.Create (1);

          YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
          YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
          phy.SetChannel (channel.Create ());

          WifiHelper wifi;
          wifi.SetStandard (WIFI_STANDARD_80211be);

          // Unique frequency per scenario
          phy.Set ("ChannelNumber", UintegerValue (channelNumber));
          phy.Set ("ChannelWidth", UintegerValue (width));
          phy.Set ("Frequency", UintegerValue (channelNumber));
          phy.Set ("GuardInterval", ns3::StringValue (gi == "short" ? "HeGuardInterval400ns" : "HeGuardInterval800ns"));
          phy.Set ("SupportedPhyBand", StringValue (freq == 2.4 ? "2_4GHz" : 
                                                    freq == 5.0 ? "5GHz" : "6GHz"));
          phy.Set ("MpduBufferSize", UintegerValue (params.mpduBufferSize));

          WifiMacHelper mac;
          Ssid ssid = Ssid ("wifi7-eval");

          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue ("HeMcs" + std::to_string (mcs)),
                                       "ControlMode", StringValue ("HeMcs0"));

          // AP
          mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
          NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

          // STAs
          mac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false));
          NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

          // Mobility
          MobilityHelper mobility;
          mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
          mobility.Install (wifiStaNodes);
          mobility.Install (wifiApNode);

          // Internet stack
          InternetStackHelper stack;
          stack.Install (wifiApNode);
          stack.Install (wifiStaNodes);

          Ipv4AddressHelper address;
          address.SetBase ("10.1.1.0", "255.255.255.0");
          Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
          Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

          // Application setup
          ApplicationContainer serverApps, clientApps;
          if (params.trafficType == "UDP" || params.trafficType == "udp")
          {
            for (uint32_t s = 0; s < params.nSta; ++s)
            {
              uint16_t port = 50000 + s;
              UdpServerHelper server (port);
              serverApps.Add (server.Install (wifiStaNodes.Get (s)));

              UdpClientHelper client (staInterfaces.GetAddress (s), port);
              client.SetAttribute ("MaxPackets", UintegerValue (0));
              client.SetAttribute ("Interval", TimeValue (MicroSeconds (300)));
              client.SetAttribute ("PacketSize", UintegerValue (params.payloadSize));
              clientApps.Add (client.Install (wifiApNode.Get (0)));
            }
          }
          else // TCP
          {
            for (uint32_t s = 0; s < params.nSta; ++s)
            {
              uint16_t port = 50000 + s;
              Address sinkLocalAddress (InetSocketAddress (staInterfaces.GetAddress (s), port));
              PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkLocalAddress);
              serverApps.Add (sink.Install (wifiStaNodes.Get (s)));

              OnOffHelper client ("ns3::TcpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (s), port));
              client.SetAttribute ("PacketSize", UintegerValue (params.payloadSize));
              client.SetAttribute ("DataRate", StringValue ("1Gbps"));
              client.SetAttribute ("MaxBytes", UintegerValue (0));
              clientApps.Add (client.Install (wifiApNode.Get (0)));
            }
          }

          serverApps.Start (Seconds (0.1));
          serverApps.Stop (params.simTime);
          clientApps.Start (Seconds (0.2));
          clientApps.Stop (params.simTime);

          // Storing RX (received) bytes per station
          std::vector<uint64_t> rxBytesPerSta (params.nSta, 0);
          std::vector<Ptr<Application>> serverAppPtrs;
          for (uint32_t j = 0; j < params.nSta; ++j)
            serverAppPtrs.push_back (serverApps.Get (j));

          // Capture server RX on app finish
          Simulator::Schedule (params.simTime - MilliSeconds (3), [&] () {
            for (uint32_t j = 0; j < params.nSta; ++j)
            {
              if (params.trafficType == "UDP" || params.trafficType == "udp")
              {
                Ptr<UdpServer> srv = DynamicCast<UdpServer> (serverAppPtrs[j]);
                rxBytesPerSta[j] = srv->GetReceived () * params.payloadSize;
              }
              else
              {
                Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverAppPtrs[j]);
                rxBytesPerSta[j] = sink->GetTotalRx ();
              }
            }
          });

          Simulator::Stop (params.simTime + MilliSeconds (10));
          Simulator::Run ();

          // Throughput calculations
          double totalRx = 0;
          for (auto rx : rxBytesPerSta) totalRx += rx;
          double throughput = (totalRx * 8.0) / (params.simTime.GetSeconds() * 1e6); // Mbps

          // Expected throughput per station (min/max checks)
          double expectedPerSta = ExpectedThroughput (mcs, width, gi, params.nSta, params.trafficType, freq);
          double minThroughput = expectedPerSta * params.nSta * (1.0 - Tolerance());
          double maxThroughput = expectedPerSta * params.nSta * (1.0 + Tolerance());
          bool error = (throughput < minThroughput || throughput > maxThroughput);

          if (error)
          {
            NS_LOG_UNCOND ("ERROR: Throughput " << throughput << " Mbps for MCS " << int(mcs)
                            << ", width " << width << ", GI " << gi << " is out of expected range ["
                            << minThroughput << ", " << maxThroughput << "] Mbps.");
            Simulator::Destroy ();
            exit (1);
          }

          SimResult result;
          result.mcs = mcs;
          result.chWidth = width;
          result.gi = gi;
          result.throughputMbps = throughput;
          result.error = false;
          results.push_back (result);

          Simulator::Destroy ();
        }
      }
    }
  }

  PrintResultsTable (results);

  return 0;
}