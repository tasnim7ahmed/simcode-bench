#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MimoWifiThroughputDistance");

struct SimulationConfig
{
  std::string accessPointMode = "ap";
  std::string stationMode = "sta";
  std::string phyStandard = "802.11n";
  double frequency = 5.0;                 // in GHz, 2.4/5.0
  uint32_t channelWidth = 20;             // MHz, can be 20/40/80
  bool shortGuardInterval = false;
  bool usePreambleDetection = true;
  bool channelBonding = false;            // For 40/80 MHz
  double step = 5.0;                      // meters
  double minDistance = 1.0;               // meters
  double maxDistance = 50.0;              // meters
  double simulationTime = 5.0;            // seconds
  bool udp = true;                        // true=UDP, false=TCP
};

struct FlowStats
{
  double throughput;
};

static std::string ResolveMcsString(uint8_t mcs) {
  std::ostringstream oss;
  oss << "HtMcs" << uint32_t(mcs);
  return oss.str();
}

static void
InstallMobility(Ptr<Node> ap, Ptr<Node> sta, double distance)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP fixed
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // Station variable
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(ap);
  mobility.Install(sta);
}

static Ipv4InterfaceContainer 
InstallInternetStack(NetDeviceContainer & staDevices, NetDeviceContainer & apDevice)
{
  InternetStackHelper stack;
  NodeContainer c;
  c.Add(staDevices.Get(0));
  c.Add(apDevice.Get(0));
  stack.Install(c);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  return address.Assign(NetDeviceContainer(staDevices.Get(0), apDevice.Get(0)));
}

static void
EnablePcap(Ptr<YansWifiPhy> phy, std::string prefix)
{
  phy->SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy->EnablePcap (prefix, phy, true, true);
}

static ApplicationContainer
InstallTrafficApp(Ptr<Node> src, Address dstAddr, bool udp, double simTime)
{
  ApplicationContainer apps;
  if (udp)
    {
      uint16_t port = 6000;
      // Install OnOff application on src
      OnOffHelper onOff ("ns3::UdpSocketFactory", dstAddr);
      onOff.SetAttribute ("PacketSize", UintegerValue(1472));
      onOff.SetAttribute ("DataRate", StringValue ("200Mbps"));
      onOff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onOff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      // Install PacketSink on destination
      apps.Add(onOff.Install(src));
    }
  else
    {
      // TCP
      uint16_t port = 50000;
      BulkSendHelper bulkSend ("ns3::TcpSocketFactory", dstAddr);
      bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
      bulkSend.SetAttribute ("SendSize", UintegerValue(1440));
      bulkSend.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      bulkSend.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      apps.Add(bulkSend.Install(src));
    }
  return apps;
}

static ApplicationContainer
InstallSinkApp(Ptr<Node> dst, bool udp, double simTime)
{
  ApplicationContainer apps;
  if (udp)
    {
      uint16_t port = 6000;
      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      apps.Add(sink.Install(dst));
    }
  else
    {
      uint16_t port = 50000;
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny (), port));
      apps.Add(sink.Install(dst));
    }
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simTime+1));
  return apps;
}

// WiFi MCS rates for 802.11n HT, up to 4 streams (0-31)
static std::vector<uint8_t>
GetMcsListForNstreams(uint8_t nStreams)
{
  // 802.11n: for 1-4 streams, each stream has 8 MCS (0-7 per stream)
  std::vector<uint8_t> mcsList;
  for (uint8_t mcs = (nStreams-1)*8; mcs < nStreams*8; ++mcs)
    {
      mcsList.push_back(mcs);
    }
  return mcsList;
}

static void
ParseCommandLine(SimulationConfig &conf)
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("udp", "Set to true for UDP, false for TCP", conf.udp);
  cmd.AddValue("freq", "Frequency GHz (2.4 or 5.0)", conf.frequency);
  cmd.AddValue("width", "Channel width (MHz, 20/40/80)", conf.channelWidth);
  cmd.AddValue("sgi", "Enable short guard interval", conf.shortGuardInterval);
  cmd.AddValue("preamble", "Enable preamble detection", conf.usePreambleDetection);
  cmd.AddValue("bonding", "Enable channel bonding (40/80 MHz)", conf.channelBonding);
  cmd.AddValue("step", "Distance step (m)", conf.step);
  cmd.AddValue("minDist", "Minimal distance (m)", conf.minDistance);
  cmd.AddValue("maxDist", "Maximal distance (m)", conf.maxDistance);
  cmd.AddValue("simTime", "Simulation time per run (s)", conf.simulationTime);
  cmd.Parse(argc, nullptr);
}

int main (int argc, char *argv[])
{
  SimulationConfig config;
  ParseCommandLine(config);

  Gnuplot gnuplot("throughput-vs-distance.png");
  gnuplot.SetTitle("802.11n Throughput vs Distance");
  gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");
  gnuplot.AppendExtra("set key outside right top box");

  // Prepare curves for all MCS x Nss
  std::vector<std::pair<Gnuplot2dDataset, std::string>> mcsCurves;

  for (uint8_t nStreams = 1; nStreams <= 4; ++nStreams)
    {
      std::vector<uint8_t> mcsList = GetMcsListForNstreams(nStreams);
      for (uint8_t const mcs : mcsList)
        {
          std::ostringstream label;
          label << nStreams << "x" << "MCS" << int(mcs % 8);
          mcsCurves.push_back(std::make_pair(Gnuplot2dDataset(), label.str()));
        }
    }

  uint32_t curveIndex = 0;

  for (uint8_t nStreams = 1; nStreams <= 4; ++nStreams)
    {
      std::vector<uint8_t> mcsList = GetMcsListForNstreams(nStreams);
      for (uint8_t const mcs : mcsList)
        {
          Gnuplot2dDataset &dataset = mcsCurves[curveIndex].first;
          dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

          for (double distance = config.minDistance; distance <= config.maxDistance; distance += config.step)
            {
              NodeContainer wifiStaNode;
              wifiStaNode.Create(1);
              NodeContainer wifiApNode;
              wifiApNode.Create(1);

              // PHY and Channel
              YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
              YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
              phy.SetChannel(channel.Create());
              phy.SetPreambleDetectionMode(config.usePreambleDetection);

              // Frequency bands
              phy.Set("ChannelWidth", UintegerValue(config.channelWidth));
              phy.Set("Frequency", UintegerValue(uint32_t(config.frequency*1000))); // MHz
              phy.Set("ShortGuardEnabled", BooleanValue(config.shortGuardInterval));

              // Wifi helper config
              WifiHelper wifi;
              wifi.SetStandard(WIFI_STANDARD_80211n);
              StringValue phyMode ("HtMcs0");
              wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                           "DataMode", StringValue(ResolveMcsString(mcs)),
                                           "ControlMode", StringValue("HtMcs0"));

              // Set number of Rx/Tx antennas for MIMO
              HtWifiMacHelper mac = HtWifiMacHelper::Default ();

              Ssid ssid = Ssid ("mimo-network");
              mac.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));
              phy.Set("Antennas", UintegerValue(nStreams));
              phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nStreams));
              phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nStreams));

              NetDeviceContainer staDevices;
              staDevices = wifi.Install(phy, mac, wifiStaNode);

              mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
              phy.Set("Antennas", UintegerValue(nStreams));
              phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nStreams));
              phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nStreams));
              NetDeviceContainer apDevice;
              apDevice = wifi.Install(phy, mac, wifiApNode);

              // Mobility
              InstallMobility(wifiApNode.Get(0), wifiStaNode.Get(0), distance);

              // Stack
              Ipv4InterfaceContainer interfaces = InstallInternetStack(staDevices, apDevice);

              // Routing not needed for point-to-point infra

              // Traffic sinks
              ApplicationContainer sinkApps = InstallSinkApp(wifiApNode.Get(0), config.udp, config.simulationTime);
              sinkApps.Start(Seconds(0.0));

              // Traffic sources
              Address apAddr(InetSocketAddress(interfaces.GetAddress(1), config.udp?6000:50000));
              ApplicationContainer sourceApps = InstallTrafficApp(wifiStaNode.Get(0), apAddr, config.udp, config.simulationTime);
              sourceApps.Start(Seconds(1.0));

              Simulator::Stop(Seconds(config.simulationTime + 1.0));
              Simulator::Run();

              // Throughput calculation
              double throughput = 0.0;
              if (config.udp)
                {
                  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
                  uint64_t totalRx = sink->GetTotalRx();
                  throughput = totalRx * 8.0 / (config.simulationTime - 1.0) / 1e6; //Mbps, substract ramp-up time
                }
              else
                {
                  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
                  uint64_t totalRx = sink->GetTotalRx();
                  throughput = totalRx * 8.0 / (config.simulationTime - 1.0) / 1e6; //Mbps
                }

              dataset.Add(distance, throughput);
              Simulator::Destroy();
            }
          std::string label = mcsCurves[curveIndex].second;
          dataset.SetLegend(label);
          ++curveIndex;
        }
    }

  for (auto const& curve : mcsCurves)
    {
      gnuplot.AddDataset(curve.first);
    }

  std::ofstream plotFile("throughput-vs-distance.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}