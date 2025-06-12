#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment {
public:
  Experiment(double distance, std::string rate);
  void Run();
  uint32_t GetPacketsReceived() const;
private:
  void SetupNodes();
  void SetupDevices();
  void SetupApplications();
  void SetupMobility();
  void OnPacketReceive(Ptr<const Packet> packet);

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  uint32_t m_receivedPackets;
  double m_distance;
  std::string m_rate;
};

Experiment::Experiment(double distance, std::string rate)
  : m_receivedPackets(0), m_distance(distance), m_rate(rate)
{
}

void Experiment::Run()
{
  SetupNodes();
  SetupDevices();
  SetupMobility();
  SetupApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

uint32_t Experiment::GetPacketsReceived() const
{
  return m_receivedPackets;
}

void Experiment::SetupNodes()
{
  m_nodes.Create(2);
  InternetStackHelper stack;
  stack.Install(m_nodes);
}

void Experiment::SetupDevices()
{
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(m_rate));

  wifiMac.SetType("ns3::StaWifiMac");
  NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, m_nodes.Get(0));

  wifiMac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, m_nodes.Get(1));

  m_devices.Add(apDevice);
  m_devices.Add(staDevice);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);
}

void Experiment::SetupApplications()
{
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(m_nodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(m_interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(m_nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&Experiment::OnPacketReceive, this));
}

void Experiment::OnPacketReceive(Ptr<const Packet> packet)
{
  ++m_receivedPackets;
}

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  std::vector<std::string> rates = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
  std::vector<double> distances = {10.0, 20.0, 30.0, 40.0, 50.0};
  Gnuplot2dDataset dataset;

  for (const auto& rate : rates)
  {
    Gnuplot2dDataset data;
    data.SetTitle(rate);
    data.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (double distance : distances)
    {
      Experiment experiment(distance, rate);
      experiment.Run();
      double rss = -80.0 - 20 * log10(distance / 10.0); // Simple path loss model approximation
      data.Add(rss, experiment.GetPacketsReceived());
    }

    dataset.AddDataset(data);
  }

  Gnuplot plot;
  plot.SetTitle("WiFi Packet Reception vs Received Signal Strength");
  plot.SetTerminal("postscript eps color");
  plot.SetOutputFilename("wifi_performance.eps");
  plot.SetLegend("Received Signal Strength (dBm)", "Packets Received");
  plot.AddDataset(dataset);

  std::ofstream plotFile("wifi_performance.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}