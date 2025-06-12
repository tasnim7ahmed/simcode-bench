#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiParameterSweep");

class WifiSimulationRunner
{
public:
  WifiSimulationRunner();
  void Run();

private:
  void SetupSimulation(uint16_t channelWidth, uint8_t mcsValue, bool enableRtsCts, bool useShortGuardInterval,
                       uint32_t trafficType, double distance);

  double m_simulationTime;
};

WifiSimulationRunner::WifiSimulationRunner()
    : m_simulationTime(10.0)
{
}

void
WifiSimulationRunner::Run()
{
  std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
  std::vector<uint8_t> mcsValues = {0, 9};
  std::vector<bool> rtsCtsOptions = {false, true};
  std::vector<bool> guardIntervals = {false, true};
  std::vector<uint32_t> trafficTypes = {0, 1}; // 0: UDP, 1: TCP
  std::vector<double> distances = {1.0, 5.0, 10.0};

  for (auto cw : channelWidths)
    {
      for (auto mcs : mcsValues)
        {
          for (auto rtsCts : rtsCtsOptions)
            {
              for (auto gi : guardIntervals)
                {
                  for (auto traffic : trafficTypes)
                    {
                      for (auto dist : distances)
                        {
                          NS_LOG_UNCOND("Running simulation with CW=" << cw << "MHz, MCS=" << (uint32_t)mcs
                                                                     << ", RTS/CTS=" << rtsCts
                                                                     << ", GI=" << gi
                                                                     << ", Traffic=" << traffic
                                                                     << ", Distance=" << dist << "m");
                          SetupSimulation(cw, mcs, rtsCts, gi, traffic, dist);
                        }
                    }
                }
            }
        }
    }
}

void
WifiSimulationRunner::SetupSimulation(uint16_t channelWidth, uint8_t mcsValue, bool enableRtsCts,
                                      bool useShortGuardInterval, uint32_t trafficType,
                                      double distance)
{
  // Nodes
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  // Spectrum Channel
  Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
  Ptr<FriisSpectrumPropagationLossModel> lossModel =
      CreateObject<FriisSpectrumPropagationLossModel>();
  spectrumChannel->AddPropagationLossModel(lossModel);
  Ptr<ConstantSpeedPropagationDelayModel> delayModel =
      CreateObject<ConstantSpeedPropagationDelayModel>();
  spectrumChannel->SetPropagationDelayModel(delayModel);

  // WiFi configuration
  WifiMacHelper mac;
  WifiPhyHelper phy;
  phy.SetChannel(spectrumChannel);
  phy.Set("ChannelWidth", UintegerValue(channelWidth));
  phy.Set("ShortGuardIntervalSupported", BooleanValue(useShortGuardInterval));
  phy.Set("PreambleDetectionThreshold", UintegerValue(1)); // For HE/VHT

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("VhtMcs" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("VhtMcs" + std::to_string(mcsValue)));

  if (enableRtsCts)
    {
      wifi.Mac().Set("RTSThreshold", UintegerValue(0));
    }
  else
    {
      wifi.Mac().Set("RTSThreshold", UintegerValue(2347));
    }

  // AP and STA configurations
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  // Applications
  uint16_t port = 9;
  ApplicationContainer serverApp;
  ApplicationContainer clientApp;

  if (trafficType == 0) // UDP
    {
      UdpServerHelper server(port);
      serverApp = server.Install(wifiApNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(Seconds(m_simulationTime));

      UdpClientHelper client(staInterface.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // Infinite packets
      client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
      client.SetAttribute("PacketSize", UintegerValue(1472));
      clientApp = client.Install(wifiStaNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(m_simulationTime));
    }
  else // TCP
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      serverApp = sink.Install(wifiApNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(Seconds(m_simulationTime));

      OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
      client.SetAttribute("PacketSize", UintegerValue(1448));
      client.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite
      clientApp = client.Install(wifiStaNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(m_simulationTime));
    }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(m_simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto& [flowId, stat] : stats)
    {
      if (stat.rxPackets > 0)
        {
          double goodput = (stat.rxBytes * 8.0) / (stat.timeLastRxPacket.GetSeconds() - stat.timeFirstTxPacket.GetSeconds()) / 1e6;
          NS_LOG_UNCOND("Flow ID: " << flowId
                                   << " Goodput: " << goodput << " Mbps"
                                   << " RX Packets: " << stat.rxPackets
                                   << " TX Packets: " << stat.txPackets
                                   << " Lost Packets: " << stat.lostPackets
                                   << " Delay Avg: " << stat.delaySum.GetSeconds() / stat.rxPackets << " s");
        }
    }

  Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("WifiParameterSweep", LOG_LEVEL_INFO);
  WifiSimulationRunner runner;
  runner.Run();
  return 0;
}