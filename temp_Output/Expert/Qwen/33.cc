#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

class ErrorModelComparison {
public:
  ErrorModelComparison(uint32_t frameSize);
  void Run();
private:
  void SetupSimulation(double snr, WifiPhyStandard phyMode, uint8_t mcs, uint32_t frameSize);
  void SendOnePacket(Ptr<Socket> socket, InetSocketAddress sinkAddress, uint32_t packetSize);
  void ReceivePacket(Ptr<Socket> socket);
  Gnuplot2dDataset GetDataSetForModel(std::string modelName);

  std::map<std::string, Gnuplot2dDataset> m_datasets;
  uint32_t m_receivedPackets;
  std::vector<std::string> m_models;
  uint32_t m_frameSize;
};

ErrorModelComparison::ErrorModelComparison(uint32_t frameSize)
  : m_receivedPackets(0),
    m_frameSize(frameSize)
{
  m_models = {"Nist", "Yans", "Table"};
  for (auto& model : m_models) {
    m_datasets[model] = Gnuplot2dDataset(model);
  }
}

void ErrorModelComparison::Run() {
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

  NetDeviceContainer devices;
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (auto& model : m_models) {
    Config::SetDefault("ns3::WifiPhy::ErrorRateModel", TypeIdValue(wifi.GetErrorRateType(model)));

    for (uint8_t mcs = 0; mcs <= 31; ++mcs) {
      for (double snrDb = 0; snrDb <= 30; snrDb += 2) {
        m_receivedPackets = 0;
        SetupSimulation(snrDb, WIFI_PHY_STANDARD_80211n_5GHZ, mcs, m_frameSize);

        devices = wifi.Install(phy, mac, nodes);
        interfaces = address.Assign(devices);

        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), 9));
        Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
        OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100000000bps")));
        onoff.SetAttribute("PacketSize", UintegerValue(m_frameSize));

        ApplicationContainer apps = onoff.Install(nodes.Get(0));
        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(1.0));

        PacketSinkHelper sink("ns3::UdpSocketFactory", sinkAddress);
        apps = sink.Install(nodes.Get(1));
        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(1.0));

        Simulator::Run();
        double fsr = static_cast<double>(m_receivedPackets) / 10.0;
        m_datasets[model].Add(snrDb, fsr);
        Simulator::Destroy();
        devices.Clear();
        interfaces.Clear();
      }
    }
  }

  Gnuplot plot;
  plot.SetTitle("Frame Success Rate vs SNR for HT MCS Rates");
  plot.SetTerminal("png size 1024,768 enhanced font 'Verdana,10'");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set key outside");

  std::ofstream plotFile("error-model-comparison.plt");
  plot.GenerateOutput(plotFile, m_datasets.begin()->second.GetTitle());
  plotFile << "plot ";
  for (size_t i = 0; i < m_models.size(); ++i) {
    if (i > 0) plotFile << ", ";
    plotFile << "\"error-model-comparison.dat\" index " << i << " title \"" << m_models[i] << "\" with linespoints";
  }
  plotFile << "\n";

  std::ofstream outFile("error-model-comparison.dat");
  for (auto& model : m_models) {
    m_datasets[model].GenerateOutput(outFile);
  }

  outFile.close();
  plotFile.close();
}

void ErrorModelComparison::SetupSimulation(double snr, WifiPhyStandard phyMode, uint8_t mcs, uint32_t frameSize) {
  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(10.0));
  phy.Set("TxPowerEnd", DoubleValue(10.0));
  phy.Set("RxNoiseFigure", DoubleValue(10.0));
  phy.Set("SnrThreshold", DoubleValue(snr));

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(phyMode);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(WifiPhy::GetHtMcsString(mcs)),
                               "ControlMode", StringValue("HtMcs0"));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
  sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9));
  sinkSocket->SetRecvCallback(MakeCallback(&ErrorModelComparison::ReceivePacket, this));

  Simulator::Stop(Seconds(1.0));
}

void ErrorModelComparison::SendOnePacket(Ptr<Socket> socket, InetSocketAddress sinkAddress, uint32_t packetSize) {
  Ptr<Packet> pkt = Create<Packet>(packetSize);
  socket->Send(pkt);
}

void ErrorModelComparison::ReceivePacket(Ptr<Socket> socket) {
  while (socket->Recv()) {
    m_receivedPackets++;
  }
}

Gnuplot2dDataset ErrorModelComparison::GetDataSetForModel(std::string modelName) {
  return m_datasets[modelName];
}

int main(int argc, char *argv[]) {
  uint32_t frameSize = 1472;
  CommandLine cmd;
  cmd.AddValue("frameSize", "The size of the frame to be transmitted", frameSize);
  cmd.Parse(argc, argv);

  ErrorModelComparison simulation(frameSize);
  simulation.Run();

  return 0;
}