#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

class ErrorModelComparison {
public:
  ErrorModelComparison(uint32_t frameSize);
  void Run();
private:
  void SetupSimulation(double snr);
  void SendOnePacket(Ptr<Socket> socket);
  void ReceivePacket(Ptr<Socket> socket);
  void CalculateFsr();
  uint32_t m_frameSize;
  std::map<std::string, std::map<uint8_t, std::vector<std::pair<double, double>>>> m_results;
  std::vector<double> m_snrValues;
};

ErrorModelComparison::ErrorModelComparison(uint32_t frameSize)
  : m_frameSize(frameSize)
{
  m_snrValues.resize(21);
  for (size_t i = 0; i < m_snrValues.size(); ++i) {
    m_snrValues[i] = -5 + i * 1.0;
  }
}

void ErrorModelComparison::Run() {
  std::vector<std::string> errorModels = {"Nist", "Yans", "Table"};
  for (const auto& model : errorModels) {
    for (uint8_t mcsIndex = 0; mcsIndex <= 76; ++mcsIndex) {
      WifiModulationClass modulationClass = WIFI_MOD_CLASS_HT;
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::" + model + "ErrorRateModel"));
      Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)));
      Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue("HtMcs0"));

      NodeContainer nodes;
      nodes.Create(2);

      YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
      YansWifiPhyHelper phy;
      phy.SetChannel(channel.Create());

      WifiMacHelper mac;
      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

      NetDeviceContainer devices;
      mac.SetType("ns3::AdhocWifiMac");
      devices = wifi.Install(phy, mac, nodes);

      MobilityHelper mobility;
      mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(0.0),
                                    "GridWidth", UintegerValue(2),
                                    "LayoutType", StringValue("RowFirst"));
      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mobility.Install(nodes);

      InternetStackHelper stack;
      stack.Install(nodes);

      Ipv4AddressHelper address;
      address.SetBase("10.0.0.0", "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign(devices);

      uint16_t port = 9;
      UdpServerHelper server(port);
      ApplicationContainer serverApps = server.Install(nodes.Get(1));
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(Seconds(10.0));

      UdpClientHelper client(interfaces.GetAddress(1), port);
      client.SetAttribute("MaxPackets", UintegerValue(100));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
      client.SetAttribute("PacketSize", UintegerValue(m_frameSize));
      ApplicationContainer clientApps = client.Install(nodes.Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(10.0));

      Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&ErrorModelComparison::ReceivePacket, this));

      for (double snr : m_snrValues) {
        SetupSimulation(snr);
        Simulator::Run();
        Simulator::Destroy();
        CalculateFsr();
      }

      Gnuplot2dDataset dataset;
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      for (size_t i = 0; i < m_snrValues.size(); ++i) {
        dataset.Add(m_snrValues[i], m_results[model][mcsIndex][i].second);
      }

      Gnuplot plot(model + "-HT-MCS" + std::to_string(mcsIndex));
      plot.AddDataset(dataset);
      plot.SetTerminal("png");
      plot.SetOutputTitle((model + "-HT-MCS" + std::to_string(mcsIndex)).c_str());
      plot.SetLegend("SNR (dB)", "Frame Success Rate");
      std::ofstream plotFile((model + "-HT-MCS" + std::to_string(mcsIndex) + ".plt").c_str());
      plot.GenerateOutput(plotFile);
      plotFile.close();
    }
  }
}

void ErrorModelComparison::SetupSimulation(double snr) {
  Config::Set("/NodeList/0/DeviceList/0/Phy/ChannelSettings/ChannelWidth", UintegerValue(20));
  Config::Set("/NodeList/0/DeviceList/0/Phy/InterferenceHelper/NoiseFigure", DoubleValue(10));
  Config::Set("/NodeList/0/DeviceList/0/Phy/State/Power", DoubleValue(1));
  Config::Set("/NodeList/0/DeviceList/0/Phy/State/SnrThreshold", DoubleValue(snr));
}

void ErrorModelComparison::SendOnePacket(Ptr<Socket> socket) {
  Ptr<Packet> packet = Create<Packet>(m_frameSize);
  socket->Send(packet);
}

void ErrorModelComparison::ReceivePacket(Ptr<Socket> socket) {
  while (socket->GetRxAvailable() > 0) {
    Ptr<Packet> packet;
    Address from;
    packet = socket->RecvFrom(from);
    if (packet) {
      // Count received packets
    }
  }
}

void ErrorModelComparison::CalculateFsr() {
  // Placeholder logic for FSR calculation
  static uint32_t sent = 0;
  static uint32_t received = 0;
  double fsr = static_cast<double>(received) / sent;
  // Store results accordingly
}

int main(int argc, char* argv[]) {
  uint32_t frameSize = 1000;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Size of application data sent in bytes", frameSize);
  cmd.Parse(argc, argv);

  ErrorModelComparison simulation(frameSize);
  simulation.Run();

  return 0;
}