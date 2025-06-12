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
  Experiment();
  void Run();
  void SetTxMode(std::string txMode);
  void SetupNodes();
  void SetupWifi();
  void SetupApplications();
  void SetupMobility();
  void HandleRx(Ptr<const Packet> packet, const Address &from);

private:
  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apIfs;
  Ipv4InterfaceContainer m_staIfs;
  uint32_t m_packetsReceived;
  std::string m_txMode;
};

Experiment::Experiment()
  : m_packetsReceived(0), m_txMode("DsssRate1Mbps") {
  m_apNode.Create(1);
  m_staNode.Create(1);
}

void Experiment::SetTxMode(std::string txMode) {
  m_txMode = txMode;
}

void Experiment::SetupNodes() {
  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);
}

void Experiment::SetupWifi() {
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(m_txMode), "ControlMode", StringValue(m_txMode));

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::ApWifiMac");
  m_apDev = wifi.Install(phy, mac, m_apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-test")));
  m_staDev = wifi.Install(phy, mac, m_staNode);
}

void Experiment::SetupMobility() {
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);
}

void Experiment::SetupApplications() {
  UdpServerHelper server(9);
  ApplicationContainer sinkApp = server.Install(m_staNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  UdpClientHelper client(m_staIfs.GetAddress(0, 0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer srcApp = client.Install(m_apNode.Get(0));
  srcApp.Start(Seconds(1.0));
  srcApp.Stop(Seconds(10.0));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&Experiment::HandleRx, this));
}

void Experiment::HandleRx(Ptr<const Packet> packet, const Address &from) {
  m_packetsReceived++;
}

void Experiment::Run() {
  SetupWifi();
  SetupMobility();
  SetupNodes();
  SetupApplications();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apIfs = address.Assign(m_apDev);
  address.SetBase("10.1.1.0", "255.255.255.0");
  m_staIfs = address.Assign(m_staDev);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  std::vector<std::string> modes = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };

  Gnuplot2dDataset dataset;
  Gnuplot gnuplot("wifi-rss-performance.eps");
  gnuplot.SetTerminal("postscript eps color enhanced");
  gnuplot.SetTitle("RSS vs Packets Received");
  gnuplot.SetLegend("Received Signal Strength (dBm)", "Packets Received");

  for (auto &mode : modes) {
    Experiment exp;
    exp.SetTxMode(mode);
    exp.Run();

    double rss = -70.0; // Example RSS value; actual simulation may vary
    dataset.Add(rss, exp.m_packetsReceived);
  }

  gnuplot.AddDataset(dataset);
  std::ofstream plotFile("wifi-rss.gnuplot");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}