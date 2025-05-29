/*
 * 802.11n MIMO Throughput vs Distance Simulation
 *
 * This ns-3 program simulates throughput vs. distance for all HT MCS (MIMO 1-4 streams)
 * modes in 802.11n. User can configure TCP/UDP, band, width, SGI, preamble, simulation time,
 * step size, and channel bonding.
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-helper.h"

#include <vector>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi11nMimoThroughputVsDistance");

struct SimConfig
{
  std::string trafficType = "TCP";    // "TCP" or "UDP"
  double simTime = 10.0;              // seconds
  double distanceStart = 1.0;         // meters
  double distanceEnd = 100.0;         // meters
  double distanceStep = 5.0;          // meters
  std::string band = "5GHz";          // "2.4GHz" or "5GHz"
  uint32_t channelWidth = 20;         // 20, 40, or 80 MHz
  bool shortGuardInterval = false;
  bool greenfieldPreamble = false;
  bool channelBonding = false;        // if true, uses widest available
};

struct RunResult
{
  double distance;
  double throughputMbps;
};

// List of valid HT MCS indices for 1-4 streams
// nStreams -> vector of MCS indices valid for that count
std::map<uint32_t, std::vector<uint8_t>> GetHtMcsMap()
{
  // Note: MCS 0-7: 1 stream, 8-15: 2 streams, 16-23: 3 streams, 24-31: 4 streams
  // For each stream, valid MCS go up to 7 (eight per stream).
  std::map<uint32_t, std::vector<uint8_t>> htMcs;
  htMcs[1] = {0,1,2,3,4,5,6,7};
  htMcs[2] = {8,9,10,11,12,13,14,15};
  htMcs[3] = {16,17,18,19,20,21,22,23};
  htMcs[4] = {24,25,26,27,28,29,30,31};
  return htMcs;
}

void ParseCommandLine (SimConfig &config)
{
  CommandLine cmd;
  cmd.AddValue ("traffic", "Traffic type: TCP or UDP", config.trafficType);
  cmd.AddValue ("simTime", "Simulation time per distance (s)", config.simTime);
  cmd.AddValue ("distanceStart", "Starting distance (m)", config.distanceStart);
  cmd.AddValue ("distanceEnd", "Ending distance (m)", config.distanceEnd);
  cmd.AddValue ("distanceStep", "Distance increment (m)", config.distanceStep);
  cmd.AddValue ("band", "Wi-Fi band: 5GHz or 2.4GHz", config.band);
  cmd.AddValue ("channelWidth", "Channel width (MHz): 20, 40, or 80", config.channelWidth);
  cmd.AddValue ("sgi", "Use short guard interval", config.shortGuardInterval);
  cmd.AddValue ("greenfield", "Enable greenfield preamble", config.greenfieldPreamble);
  cmd.AddValue ("channelBonding", "Enable channel bonding (widest channel)", config.channelBonding);
  cmd.Parse (GetArgc(), GetArgv());
}

Ptr<Application> InstallTrafficGenerator(const std::string &trafficType,
                                         Ptr<Node> sender, Address sinkAddress,
                                         uint16_t port, double simTime)
{
  if (trafficType == "UDP" || trafficType == "udp")
    {
      uint32_t packetSize = 1472; // bytes (typical)
      uint32_t maxPacketCount = 0; // unlimited
      DataRate dataRate ("1000Mbps");
      OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
      onoff.SetAttribute ("DataRate", DataRateValue (dataRate));
      onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
      onoff.SetAttribute ("MaxBytes", UintegerValue (0));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (0.1)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      return onoff.Install (sender).Get (0);
    }
  else
    {
      BulkSendHelper bulkSend ("ns3::TcpSocketFactory", sinkAddress);
      bulkSend.SetAttribute ("MaxBytes", UintegerValue (0));
      bulkSend.SetAttribute ("SendSize", UintegerValue (1448)); // default segment size
      bulkSend.SetAttribute ("StartTime", TimeValue (Seconds (0.1)));
      bulkSend.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      return bulkSend.Install (sender).Get (0);
    }
}

int main (int argc, char *argv[])
{
  SimConfig config;
  ParseCommandLine (config);

  Gnuplot plot ("wifi11n-mimo-throughput-vs-distance.pdf");
  plot.SetTitle ("802.11n MIMO Throughput vs Distance");
  plot.SetLegend ("Distance (meters)", "Throughput (Mbps)");

  auto mcsPerStream = GetHtMcsMap ();

  // Build one plot for each {numStreams, MCS}
  std::vector<Gnuplot2dDataset> datasets;
  std::vector<std::string> datasetLegends;

  std::map<std::string, std::vector<RunResult>> allResults;

  for (uint32_t nStreams = 1; nStreams <= 4; ++nStreams)
    {
      auto &mcsList = mcsPerStream.at (nStreams);
      for (auto mcs : mcsList)
        {
          // Build a legend label
          std::ostringstream oss;
          oss << nStreams << "x" << nStreams << " MCS" << (uint32_t)mcs;
          std::string legendLabel = oss.str();
          datasetLegends.push_back (legendLabel);
          allResults[legendLabel] = std::vector<RunResult> ();
        }
    }

  // For each {stream, MCS}, simulate throughput for each distance
  uint32_t legendIdx = 0;
  for (uint32_t nStreams = 1; nStreams <= 4; ++nStreams)
    {
      const auto &mcsList = mcsPerStream.at(nStreams);
      for (auto mcs : mcsList)
        {
          std::string legend = datasetLegends[legendIdx++];
          std::vector<RunResult> &results = allResults[legend];

          for (double distance = config.distanceStart; distance <= config.distanceEnd + 1e-6; distance += config.distanceStep)
            {
              Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200")); // Disable RTS/CTS

              NodeContainer wifiStaNode, wifiApNode;
              wifiStaNode.Create (1);
              wifiApNode.Create (1);

              // Physical channel
              YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
              YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
              phy.SetChannel (channel.Create ());

              double freq = (config.band == "2.4GHz") ? 2.4 : 5.0;
              if (config.band == "2.4GHz")
                {
                  phy.Set ("Frequency", UintegerValue (2412)); // Channel 1
                }
              else
                {
                  phy.Set ("Frequency", UintegerValue (5180)); // Channel 36
                }

              phy.Set ("ChannelWidth", UintegerValue (config.channelWidth));
              phy.Set ("ShortGuardEnabled", BooleanValue (config.shortGuardInterval));
              phy.Set ("Greenfield", BooleanValue (config.greenfieldPreamble));

              // Channel bonding
              if (config.channelBonding)
                {
                  phy.Set ("ChannelWidth", UintegerValue (80)); // Max for 802.11n: 40, but for 11ac: 80, just for flexibility
                }

              // WiFi (802.11n) configuration
              WifiHelper wifi;
              wifi.SetStandard ((config.band == "2.4GHz") ? WIFI_STANDARD_80211n : WIFI_STANDARD_80211n);

              WifiMacHelper mac;
              Ssid ssid = Ssid ("ns3-80211n");

              // Set HT capabilities and stream count
              HtWifiMacHelper htMac;
              htMac.SetType ("ns3::StaWifiMac",
                             "Ssid", SsidValue (ssid),
                             "ActiveProbing", BooleanValue (false));
              HtWifiMacHelper htApMac;
              htApMac.SetType ("ns3::ApWifiMac",
                               "Ssid", SsidValue (ssid));

              phy.Set ("Antennas", UintegerValue (nStreams));
              phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (nStreams));
              phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (nStreams));
              phy.Set ("TxSpatialStreams", UintegerValue (nStreams));
              phy.Set ("RxSpatialStreams", UintegerValue (nStreams));

              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                           "DataMode", StringValue ("HtMcs" + std::to_string ((uint32_t)mcs)),
                                           "ControlMode", StringValue ("HtMcs0"));

              // Devices
              NetDeviceContainer staDevice, apDevice;
              staDevice = wifi.Install (phy, htMac, wifiStaNode);
              apDevice = wifi.Install (phy, htApMac, wifiApNode);

              // Mobility: AP at (0,0,0), STA at (distance,0,0)
              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
              positionAlloc->Add (Vector (distance, 0.0, 0.0));
              positionAlloc->Add (Vector (0.0, 0.0, 0.0));
              mobility.SetPositionAllocator (positionAlloc);
              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
              mobility.Install (wifiStaNode);
              mobility.Install (wifiApNode);

              // Internet Stack
              InternetStackHelper stack;
              stack.Install (wifiStaNode);
              stack.Install (wifiApNode);

              Ipv4AddressHelper address;
              address.SetBase ("10.1.1.0", "255.255.255.0");
              Ipv4InterfaceContainer staInterface, apInterface;
              staInterface = address.Assign (staDevice);
              apInterface = address.Assign (apDevice);

              // Sink: AP
              uint16_t port = 9;
              ApplicationContainer sinkApp;

              if (config.trafficType == "UDP" || config.trafficType == "udp")
                {
                  UdpServerHelper udpServer (port);
                  sinkApp = udpServer.Install (wifiApNode.Get (0));
                }
              else
                {
                  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                                     InetSocketAddress (Ipv4Address::GetAny (), port));
                  sinkApp = packetSinkHelper.Install (wifiApNode.Get (0));
                }
              sinkApp.Start (Seconds (0.0));
              sinkApp.Stop (Seconds (config.simTime + 1.0));

              // Sender: STA
              Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), port));
              Ptr<Application> app = InstallTrafficGenerator (config.trafficType, wifiStaNode.Get (0), sinkAddress, port, config.simTime);
              ApplicationContainer appCont (app);
              appCont.Start (Seconds (0.2));
              appCont.Stop (Seconds (config.simTime + 0.1));

              FlowMonitorHelper flowmon;
              Ptr<FlowMonitor> monitor = flowmon.InstallAll();

              Simulator::Stop (Seconds (config.simTime + 0.5));
              Simulator::Run ();

              // Measure throughput
              double throughput = 0.0;
              monitor->CheckForLostPackets ();
              Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
              std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

              for (auto &flow : stats)
                {
                  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
                  // Only outgoing from STA to AP
                  if (t.sourceAddress == staInterface.GetAddress (0) &&
                      t.destinationAddress == apInterface.GetAddress (0))
                    {
                      double totalBytes = flow.second.rxBytes * 8.0;
                      double timeSecs = config.simTime;
                      throughput = (totalBytes / timeSecs) / 1e6; // Mbps
                    }
                }

              results.push_back (RunResult {distance, throughput});

              Simulator::Destroy ();
            }
        }
    }

  // Now, add each result set to the plot
  legendIdx = 0;
  for (uint32_t nStreams = 1; nStreams <= 4; ++nStreams)
    {
      const auto &mcsList = mcsPerStream.at(nStreams);
      for (auto mcs : mcsList)
        {
          std::string legend = datasetLegends[legendIdx++];
          Gnuplot2dDataset dataset;
          dataset.SetTitle (legend);
          dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
          for (const auto &res : allResults.at(legend))
            {
              dataset.Add (res.distance, res.throughputMbps);
            }
          plot.AddDataset (dataset);
        }
    }

  std::ofstream plotFile ("wifi11n-mimo-throughput-vs-distance.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();
  std::cout << "Simulation complete. Plot saved as wifi11n-mimo-throughput-vs-distance.pdf" << std::endl;
  return 0;
}