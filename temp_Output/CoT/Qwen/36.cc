#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateValidation");

class VhtErrorRateExperiment
{
public:
  VhtErrorRateExperiment();
  void Run(uint32_t frameSize);
private:
  void SetupSimulation();
  void SendOnePacket(Ptr<Socket> socket);
  void ReceivePacket(Ptr<Socket> socket);
  uint32_t m_receivedPackets;
  uint32_t m_sentPackets;
  uint32_t m_frameSize;
  Gnuplot2dDataset m_nistData;
  Gnuplot2dDataset m_yansData;
  Gnuplot2dDataset m_tableData;
};

VhtErrorRateExperiment::VhtErrorRateExperiment()
  : m_receivedPackets(0),
    m_sentPackets(0),
    m_frameSize(1472),
    m_nistData("NIST"),
    m_yansData("YANS"),
    m_tableData("Table-Based")
{
}

void
VhtErrorRateExperiment::Run(uint32_t frameSize)
{
  m_frameSize = frameSize;

  std::vector<Gnuplot2dDataset*> datasets;
  datasets.push_back(&m_nistData);
  datasets.push_back(&m_yansData);
  datasets.push_back(&m_tableData);

  Gnuplot gnuplot("vht-error-rate-validation.plt");
  gnuplot.SetTitle("Frame Success Rate vs SNR for VHT MCS (Excluding MCS9 20MHz)");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
  gnuplot.AppendExtra("set yrange [0:1]");
  gnuplot.AppendExtra("set key outside");

  for (double snrDb = -5.0; snrDb <= 30.0; snrDb += 2.0)
    {
      NS_LOG_UNCOND("Running at SNR: " << snrDb << " dB");

      WifiModeList vhtMcsList = WifiPhy::GetStaticModes();

      for (const auto& mode : vhtMcsList)
        {
          std::string modeName = mode.GetUniqueName();
          if (modeName.find("VhtMcs9") != std::string::npos &&
              modeName.find("20MHz") != std::string::npos)
            {
              continue; // Skip VHT MCS9 for 20 MHz
            }

          NS_LOG_UNCOND("Testing MCS: " << modeName);

          Config::SetDefault("ns3::WifiRemoteStationManager::ShortGuardInterval", BooleanValue(false));
          Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue(modeName));
          Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue(modeName));

          SetupSimulation();

          m_receivedPackets = 0;
          m_sentPackets = 0;

          Simulator::Schedule(Seconds(0.1), &VhtErrorRateExperiment::SendOnePacket, this, m_senderSocket);

          Simulator::Run();
          Simulator::Destroy();

          double fsr = static_cast<double>(m_receivedPackets) / m_sentPackets;

          if (modeName.find("Nist") != std::string::npos)
            {
              m_nistData.Add(snrDb, fsr);
            }
          else if (modeName.find("Yans") != std::string::npos)
            {
              m_yansData.Add(snrDb, fsr);
            }
          else
            {
              m_tableData.Add(snrDb, fsr);
            }
        }
    }

  gnuplot.GenerateOutput(std::ofstream("vht-error-rate-validation.gp"));

  std::ofstream plotFile("vht-fsr-vs-snr.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

void
VhtErrorRateExperiment::SetupSimulation()
{
  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid("vht-error-rate-test");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
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
  interfaces.Add(address.Assign(apDevices));

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(m_frameSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(10.0));

  m_senderSocket = Socket::CreateSocket(nodes.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress remote(interfaces.GetAddress(0), port);
  m_senderSocket->Connect(remote);

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx",
                               MakeCallback(&VhtErrorRateExperiment::ReceivePacket, this));
}

void
VhtErrorRateExperiment::SendOnePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet = Create<Packet>(m_frameSize);
  socket->Send(packet);
  m_sentPackets++;
}

void
VhtErrorRateExperiment::ReceivePacket(Ptr<Socket> socket)
{
  m_receivedPackets++;
}

int
main(int argc, char *argv[])
{
  uint32_t frameSize = 1472;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Size of application frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  VhtErrorRateExperiment experiment;
  experiment.Run(frameSize);

  return 0;
}