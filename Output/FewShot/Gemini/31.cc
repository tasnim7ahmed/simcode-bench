#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  double simulationTime = 10; //seconds
  double distance = 10; //meters
  uint32_t packetSize = 1472; //bytes
  double minSnr = 0;
  double maxSnr = 30;
  uint32_t numSnrPoints = 31;
  std::string rate = "He80MHz";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("packetSize", "Size of packet in bytes", packetSize);
  cmd.AddValue("minSnr", "Minimum SNR value", minSnr);
  cmd.AddValue("maxSnr", "Maximum SNR value", maxSnr);
  cmd.AddValue("numSnrPoints", "Number of SNR points", numSnrPoints);
  cmd.AddValue("rate", "Rate", rate);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211be);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.Set("RxGain", DoubleValue(-10));
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelay");
  channel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-80));
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MilliSeconds(1024)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(100000000));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  if (tracing) {
    phy.EnablePcap("eht-validation", apDevices);
  }

  Gnuplot2dDataset datasetNist;
  datasetNist.SetTitle("Nist");
  datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset datasetYans;
  datasetYans.SetTitle("Yans");
  datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset datasetTable;
  datasetTable.SetTitle("Table");
  datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  Gnuplot plot("eht-validation.png");
  plot.SetTitle("Frame Success Rate vs SNR");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");

  for (uint32_t mcs = 0; mcs <= 11; ++mcs) {
    std::stringstream ss;
    ss << rate << "Mcs" << mcs;
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(80));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/Frequency", UintegerValue(5180));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/Standard", EnumValue(WIFI_PHY_STANDARD_80211be));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/HeMcs", UintegerValue(mcs));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/RemoteStationManager/HeMcs", UintegerValue(mcs));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/RemoteStationManager/NonErHeMcs", UintegerValue(mcs));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/RemoteStationManager/ErHeMcs", UintegerValue(mcs));
    for (uint32_t i = 0; i < numSnrPoints; ++i) {
      double snr = minSnr + (maxSnr - minSnr) * i / (numSnrPoints - 1);
      double rss = -80 + snr;
      channel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
      Simulator::Stop(Seconds(simulationTime));
      Simulator::Run();

      Ptr<UdpClient> clientApp = DynamicCast<UdpClient>(clientApps.Get(0));
      uint64_t packetsSent = clientApp->GetPacketsSent();
      Ptr<UdpServer> serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
      uint64_t packetsReceived = serverApp->GetPacketsReceived();

      double successRate = (double)packetsReceived / packetsSent;

      datasetNist.Add(snr, successRate);
      datasetYans.Add(snr, successRate);
      datasetTable.Add(snr, successRate);
    }
  }

  plot.AddDataset(datasetNist);
  plot.AddDataset(datasetYans);
  plot.AddDataset(datasetTable);
  plot.Plot();

  Simulator::Destroy();
  return 0;
}