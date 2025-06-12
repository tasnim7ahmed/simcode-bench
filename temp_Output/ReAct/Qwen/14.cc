#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWi-FiNetwork");

class QosWifiExperiment
{
public:
  QosWifiExperiment();
  void Setup(uint32_t payloadSize, uint32_t simulationTime, double distance, bool enableRtsCts, bool pcapEnabled);

private:
  void Report();

  uint32_t m_payloadSize;
  uint32_t m_simulationTime;
  double m_distance;
  bool m_enableRtsCts;
  bool m_pcapEnabled;

  std::vector<Ptr<WifiNetDevice>> m_staDevices;
  std::vector<Ptr<WifiNetDevice>> m_apDevices;
};

QosWifiExperiment::QosWifiExperiment()
{
}

void
QosWifiExperiment::Setup(uint32_t payloadSize, uint32_t simulationTime, double distance,
                         bool enableRtsCts, bool pcapEnabled)
{
  m_payloadSize = payloadSize;
  m_simulationTime = simulationTime;
  m_distance = distance;
  m_enableRtsCts = enableRtsCts;
  m_pcapEnabled = pcapEnabled;

  NodeContainer apNodes;
  NodeContainer staNodes;
  apNodes.Create(4);
  staNodes.Create(4);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  if (m_enableRtsCts)
    {
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                                   StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    }
  else
    {
      wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    }

  WifiMacHelper mac;
  Ssid ssid;
  NetDeviceContainer apDev;
  NetDeviceContainer staDev;

  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream oss;
      oss << "network-" << i;
      ssid = Ssid(oss.str());
      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2.5)));

      apDev = wifi.Install(phy, mac, apNodes.Get(i));
      mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

      staDev = wifi.Install(phy, mac, staNodes.Get(i));

      m_apDevices.push_back(apDev.Get(0)->GetObject<WifiNetDevice>());
      m_staDevices.push_back(staDev.Get(0)->GetObject<WifiNetDevice>());
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> apInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfaces;

  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream network;
      network << "10.1." << i << ".0";
      address.SetBase(network.str().c_str(), "255.255.255.0");

      Ipv4InterfaceContainer apIf = address.Assign(apDev);
      Ipv4InterfaceContainer staIf = address.Assign(staDev);

      apInterfaces.push_back(apIf);
      staInterfaces.push_back(staIf);
    }

  UdpServerHelper server(9);
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 4; ++i)
    {
      serverApps.Add(server.Install(apNodes.Get(i)));
    }
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(m_simulationTime));

  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(m_payloadSize));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 4; ++i)
    {
      Address sinkAddress(InetSocketAddress(apInterfaces[i].GetAddress(0), 9));
      onoff.SetAttribute("Remote", AddressValue(sinkAddress));
      clientApps.Add(onoff.Install(staNodes.Get(i)));
    }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(m_simulationTime - 1));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/Queue/Enqueue",
                  MakeCallback(&QosWifiExperiment::Report));

  if (m_pcapEnabled)
    {
      phy.EnablePcapAll("qos_wifi_experiment");
    }
}

void
QosWifiExperiment::Report()
{
  for (auto& sta : m_staDevices)
    {
      auto edca = sta->GetMac()->GetObject<RegularWifiMac>()->GetEdca(AC_BE);
      NS_LOG_UNCOND("TXOP Duration AC_BE: " << edca->GetTxopLimit());
      edca = sta->GetMac()->GetObject<RegularWifiMac>()->GetEdca(AC_VI);
      NS_LOG_UNCOND("TXOP Duration AC_VI: " << edca->GetTxopLimit());
    }
}

int
main(int argc, char* argv[])
{
  uint32_t payloadSize = 1024;
  uint32_t simulationTime = 10;
  double distance = 10.0;
  bool enableRtsCts = false;
  bool pcapEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("pcapEnabled", "Enable PCAP file generation", pcapEnabled);
  cmd.Parse(argc, argv);

  QosWifiExperiment experiment;
  experiment.Setup(payloadSize, simulationTime, distance, enableRtsCts, pcapEnabled);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}