#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodputSimulation");

class WifiSimulationRunner
{
public:
  WifiSimulationRunner();
  void Run();

private:
  void SetupSimulation(uint16_t channelWidth, uint8_t mcsValue, bool enableRtsCts, bool useTcp, double distance);
  void ReportResults();
  void SendPacket(Ptr<Socket> socket, Ptr<Packet> packet);

  double m_distance;
  double m_goodput;
  uint16_t m_channelWidth;
  uint8_t m_mcsValue;
  bool m_enableRtsCts;
  bool m_useTcp;
};

WifiSimulationRunner::WifiSimulationRunner()
    : m_distance(0.0), m_goodput(0.0), m_channelWidth(0), m_mcsValue(0),
      m_enableRtsCts(false), m_useTcp(false)
{
}

void WifiSimulationRunner::Run()
{
  std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
  std::vector<uint8_t> mcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<bool> rtsCtsOptions = {false, true};
  std::vector<bool> transportProtocols = {false, true}; // false=UDP, true=TCP
  std::vector<double> distances = {1.0, 5.0, 10.0, 20.0, 50.0};

  for (auto distance : distances)
  {
    for (auto cw : channelWidths)
    {
      for (auto mcs : mcsValues)
      {
        for (auto rtsCts : rtsCtsOptions)
        {
          for (auto tcp : transportProtocols)
          {
            SetupSimulation(cw, mcs, rtsCts, tcp, distance);
            Simulator::Run();
            Simulator::Destroy();
            ReportResults();
          }
        }
      }
    }
  }
}

void WifiSimulationRunner::SetupSimulation(uint16_t channelWidth, uint8_t mcsValue,
                                          bool enableRtsCts, bool useTcp, double distance)
{
  m_channelWidth = channelWidth;
  m_mcsValue = mcsValue;
  m_enableRtsCts = enableRtsCts;
  m_useTcp = useTcp;
  m_distance = distance;

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiPhyHelper phy;
  phy.SetErrorRateModel("ns3::NistErrorRateModel");
  phy.SetChannelWidth(channelWidth);
  phy.Set("ShortGuardInterval", BooleanValue(mcsValue >= 8)); // Assuming short GI if MCS >=8

  SpectrumChannelHelper channelHelper = SpectrumChannelHelper::Default();
  channelHelper.SetChannel("ns3::MultiModelSpectrumChannel");
  phy.SetChannel(channelHelper.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("VhtMcs" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("VhtMcs0"));

  if (enableRtsCts)
  {
    mac.SetRTSThreshold(100);
  }
  else
  {
    mac.SetRTSThreshold(99999);
  }

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns-3-ssid")));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns-3-ssid")),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

  Address sinkAddress;
  uint16_t port = 9;

  if (useTcp)
  {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiStaNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1400));

    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    sinkAddress = InetSocketAddress(staInterfaces.GetAddress(0), port);
  }
  else
  {
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1400));

    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(
                      [](Ptr<const Packet> p, const Address &) {
                        m_goodput += p->GetSize();
                      }));
}

void WifiSimulationRunner::ReportResults()
{
  std::ofstream outFile("wifi_goodput.csv", std::ios_base::app);
  if (!outFile.is_open())
  {
    NS_FATAL_ERROR("Could not open output file.");
  }

  outFile << m_distance << "," << m_channelWidth << "," << (int)m_mcsValue << ","
          << m_enableRtsCts << "," << m_useTcp << "," << m_goodput / 10 << "\n";
  outFile.close();
  m_goodput = 0;
}

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiGoodputSimulation", LOG_LEVEL_INFO);
  WifiSimulationRunner runner;
  runner.Run();
  return 0;
}