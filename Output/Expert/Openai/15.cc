#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughputVsDistance");

struct SimResult
{
  double distance;
  double throughput; // Mbps
};

struct PlotData
{
  std::string legend;
  std::vector<SimResult> results;
};

static void
CalculateThroughput(Ptr<Application> app, uint64_t *lastRx, Time *lastTime, double *throughput)
{
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
  uint64_t totalRx = sink->GetTotalRx();
  Time now = Simulator::Now();
  if (*lastTime != Seconds(0))
    {
      double deltaTime = (now - *lastTime).GetSeconds();
      double deltaRx = static_cast<double>(totalRx - *lastRx) * 8.0 / 1e6; // Mb
      *throughput = deltaRx / deltaTime; // Mbps
    }
  *lastRx = totalRx;
  *lastTime = now;
}

int main(int argc, char *argv[])
{
  std::string phyMode = "HtMcs7";
  double simTime = 10.0;
  double distanceMin = 1.0;
  double distanceMax = 50.0;
  double distanceStep = 5.0;
  uint32_t channelWidth = 20;
  bool shortGuard = false;
  bool use2_4Ghz = true;
  bool enableSgi = false;
  bool useUdp = true;
  uint32_t maxMcs = 31;
  uint32_t nStreamsMax = 4;
  bool channelBonding = false;
  bool enablePreambleDetection = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("distanceMin", "Minimum distance (m)", distanceMin);
  cmd.AddValue("distanceMax", "Maximum distance (m)", distanceMax);
  cmd.AddValue("distanceStep", "Step size for distance (m)", distanceStep);
  cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue("shortGuard", "Enable short guard interval", shortGuard);
  cmd.AddValue("use2_4Ghz", "Use 2.4 GHz band (false selects 5 GHz)", use2_4Ghz);
  cmd.AddValue("useUdp", "Use UDP (if false, use TCP)", useUdp);
  cmd.AddValue("nStreamsMax", "Maximum number of MIMO streams to test", nStreamsMax);
  cmd.AddValue("channelBonding", "Enable 40MHz channel bonding", channelBonding);
  cmd.AddValue("enablePreambleDetection", "Enable preamble detection", enablePreambleDetection);
  cmd.Parse(argc, argv);

  if (channelBonding)
    {
      channelWidth = 40;
    }
  else
    {
      if (channelWidth > 20)
        channelWidth = 20;
    }

  uint32_t mcsMaxTable[5] = {7, 15, 23, 31, 31}; // nStreams 1..4
  std::vector<PlotData> allPlots;

  for (uint32_t nStreams = 1; nStreams <= nStreamsMax; ++nStreams)
    {
      uint32_t mcsMax = mcsMaxTable[nStreams];
      for (uint32_t mcs = 0; mcs <= mcsMax; ++mcs)
        {
          std::ostringstream legend;
          legend << nStreams << "x" << "MCS" << mcs;
          PlotData pd;
          pd.legend = legend.str();

          for (double distance = distanceMin; distance <= distanceMax + 1e-6; distance += distanceStep)
            {
              NodeContainer nodes;
              nodes.Create(2); // AP and STA

              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
              positionAlloc->Add(Vector(0.0, 0.0, 0.0));
              positionAlloc->Add(Vector(distance, 0.0, 0.0));
              mobility.SetPositionAllocator(positionAlloc);
              mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
              mobility.Install(nodes);

              WifiHelper wifi;
              wifi.SetStandard(WIFI_STANDARD_80211n);

              YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
              channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
              channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel","ReferenceDistance", DoubleValue(1.0),"Exponent", DoubleValue(3.0));

              YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
              phy.SetChannel(channel.Create());
              phy.Set("ShortGuardEnabled", BooleanValue(shortGuard));
              phy.Set("ChannelWidth", UintegerValue(channelWidth));
              phy.Set("Greenfield", BooleanValue(false));
              phy.Set("EnablePreambleDetection", BooleanValue(enablePreambleDetection));
              if (use2_4Ghz)
                phy.Set("Frequency", UintegerValue(2412));
              else
                phy.Set("Frequency", UintegerValue(5180));

              WifiMacHelper mac;
              Ssid ssid = Ssid("mimo-ssid");

              mac.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));
              NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(1));

              mac.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid));
              NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(0));

              HtWifiMacHelper htMac;
              uint8_t rxAntennas = nStreams, txAntennas = nStreams;
              phy.Set("Antennas", UintegerValue(nStreams));
              phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nStreams));
              phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nStreams));

              wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                          "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                                          "ControlMode", StringValue("HtMcs0"),
                                          "RtsCtsThreshold", UintegerValue(999999));

              InternetStackHelper stack;
              stack.Install(nodes);

              Ipv4AddressHelper address;
              address.SetBase("192.168.1.0", "255.255.255.0");
              Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevice));

              uint16_t port = 5000;
              ApplicationContainer sinkApp, sourceApp;

              if (useUdp)
                {
                  UdpServerHelper udpServer(port);
                  sinkApp = udpServer.Install(nodes.Get(0));
                  sinkApp.Start(Seconds(0.0));
                  sinkApp.Stop(Seconds(simTime + 0.2));

                  UdpClientHelper udpClient(interfaces.GetAddress(0), port);
                  udpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                  udpClient.SetAttribute("Interval", TimeValue(Time("0.00002s")));
                  udpClient.SetAttribute("PacketSize", UintegerValue(1472));
                  sourceApp = udpClient.Install(nodes.Get(1));
                  sourceApp.Start(Seconds(1.0));
                  sourceApp.Stop(Seconds(simTime));
                }
              else
                {
                  uint32_t sinkSize = 100000000;
                  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
                  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
                  sinkApp = packetSinkHelper.Install(nodes.Get(0));
                  sinkApp.Start(Seconds(0.0));
                  sinkApp.Stop(Seconds(simTime + 0.2));

                  OnOffHelper onoff("ns3::TcpSocketFactory", Address());
                  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                  onoff.SetAttribute("PacketSize", UintegerValue(1472));
                  onoff.SetAttribute("DataRate", DataRateValue(DataRate("400Mbps")));
                  AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(0), port));
                  onoff.SetAttribute("Remote", remoteAddress);
                  sourceApp = onoff.Install(nodes.Get(1));
                  sourceApp.Start(Seconds(1.0));
                  sourceApp.Stop(Seconds(simTime));
                }

              Ipv4GlobalRoutingHelper::PopulateRoutingTables();

              uint64_t lastRx = 0;
              Time lastTime = Seconds(0);
              double throughput = 0.0;
              Simulator::Stop(Seconds(simTime + 0.2));
              Simulator::Run();
              ApplicationContainer apps = sinkApp;
              Ptr<Application> app = apps.Get(0);
              lastRx = 0;
              lastTime = Seconds(0);
              throughput = 0.0;
              CalculateThroughput(app, &lastRx, &lastTime, &throughput);

              // Instead, get sink stats from totalRx/interval
              Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
              uint64_t totalBytes = sink->GetTotalRx();
              double mbits = totalBytes * 8.0 / 1e6;
              double realThroughput = mbits / simTime;
              pd.results.push_back({distance, realThroughput});
              Simulator::Destroy();
            }
          allPlots.push_back(pd);
        }
    }

  std::string dataFileName = "wifi-mimo-throughput-vs-distance.plt";
  std::string plotFileName = "wifi-mimo-throughput-vs-distance.png";
  Gnuplot plot(plotFileName);
  plot.SetTitle("802.11n Throughput vs. Distance");
  plot.SetTerminal("png");
  plot.SetLegend("Distance (m)", "Throughput (Mbps)");

  for (const auto &pd : allPlots)
    {
      Gnuplot2dDataset dataset;
      dataset.SetTitle(pd.legend);
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      for (const auto &res : pd.results)
        {
          dataset.Add(res.distance, res.throughput);
        }
      plot.AddDataset(dataset);
    }
  std::ofstream plotFile(dataFileName);
  plot.GenerateOutput(plotFile);
  plotFile.close();

  std::cout << "Simulation finished. Plot data generated to '" << dataFileName << "' and will be plotted as '" << plotFileName << "'." << std::endl;
  return 0;
}