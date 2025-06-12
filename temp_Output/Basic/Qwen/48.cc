#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiPhyTest");

class WifiPhyTest {
public:
  WifiPhyTest();
  void Run (double distance, double simulationTime, uint8_t mcsValue, uint16_t channelWidth, bool useShortGuardInterval,
            bool useSpectrumModel, bool useTcp, bool pcapEnabled);

private:
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  Ipv4InterfaceContainer staInterfaces;

  void CalculateThroughput ();
  void ReportSignalNoise (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, 
                          const WifiMacHeader* header, RxPowerWattPerChannelBand signal, 
                          RxPowerWattPerChannelBand noise, Ptr<PhyEntity> entity);
};

WifiPhyTest::WifiPhyTest()
{
  wifiStaNode.Create(1);
  wifiApNode.Create(1);
}

void
WifiPhyTest::Run(double distance, double simulationTime, uint8_t mcsValue, uint16_t channelWidth, 
                 bool useShortGuardInterval, bool useSpectrumModel, bool useTcp, bool pcapEnabled)
{
  // Setup mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // AP
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  // Create Wi-Fi helper
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  // Configure PHY
  WifiPhyHelper phy;
  if (useSpectrumModel)
    {
      phy = SpectrumWifiPhyHelper::Default();
    }
  else
    {
      phy = YansWifiPhyHelper::Default();
    }

  // Channel configuration
  WifiChannelHelper channel = WifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  // MAC configuration
  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-test");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(phy, mac, wifiStaNode.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode.Get(0));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  // Connect signal/noise trace
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxSignalNoiseInfo",
                  MakeBoundCallback(&WifiPhyTest::ReportSignalNoise, this));

  // Traffic application
  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper(useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  OnOffHelper onoff(useTcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                    InetSocketAddress(staInterfaces.GetAddress(0), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer srcApp = onoff.Install(wifiApNode.Get(0));
  srcApp.Start(Seconds(1.0));
  srcApp.Stop(Seconds(simulationTime - 0.1));

  // PCAP capture
  if (pcapEnabled)
    {
      phy.EnablePcap("wifi-phy-test", apDevice.Get(0));
    }

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  CalculateThroughput();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter->first);
      if (t.sourceAddress == apInterface.GetAddress(0) && t.destinationAddress == staInterfaces.GetAddress(0))
        {
          std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
          std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1e6 << " Mbps\n";
        }
    }

  Simulator::Destroy();
}

void
WifiPhyTest::CalculateThroughput ()
{
  static uint64_t lastTotalRx = 0;
  static Time lastTime;
  uint64_t totalRx = DynamicCast<PacketSink>(sinkInterfaces.Get(0)->GetApplication(0))->GetTotalRx();
  Time now = Simulator::Now();
  double diff = (now - lastTime).GetSeconds();
  if (diff > 0)
    {
      double throughput = (totalRx - lastTotalRx) * 8.0 / diff / 1e6;
      NS_LOG_UNCOND("Time " << now.GetSeconds() << "s, Throughput = " << throughput << " Mbps");
    }
  lastTotalRx = totalRx;
  lastTime = now;
  Simulator::Schedule(Seconds(1), &WifiPhyTest::CalculateThroughput, this);
}

void
WifiPhyTest::ReportSignalNoise (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                                const WifiMacHeader* header, RxPowerWattPerChannelBand signal,
                                RxPowerWattPerChannelBand noise, Ptr<PhyEntity> entity)
{
  double signalDbm = 10 * log10(signal.w) + 30; // W to dBm
  double noiseDbm = 10 * log10(noise.w) + 30;
  double snr = signal.w / noise.w;
  double snrDb = 10 * log10(snr);
  NS_LOG_INFO("Signal: " << signalDbm << " dBm, Noise: " << noiseDbm << " dBm, SNR: " << snrDb << " dB");
}

int main(int argc, char *argv[])
{
  double distance = 10.0;
  double simulationTime = 10.0;
  uint8_t mcsValue = 7;
  uint16_t channelWidth = 20;
  bool useShortGuardInterval = false;
  bool useSpectrumModel = true;
  bool useTcp = false;
  bool pcapEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance in meters between nodes", distance);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("mcsValue", "MCS index value (0-31)", mcsValue);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue("useShortGuardInterval", "Enable/disable short guard interval", useShortGuardInterval);
  cmd.AddValue("useSpectrumModel", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrumModel);
  cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
  cmd.AddValue("pcapEnabled", "Enable PCAP capture", pcapEnabled);
  cmd.Parse(argc, argv);

  // MCS validation
  if (mcsValue > 31)
    {
      NS_FATAL_ERROR("Invalid MCS value: " << mcsValue);
    }

  // Channel width validation
  if (channelWidth != 20 && channelWidth != 40 && channelWidth != 80)
    {
      NS_FATAL_ERROR("Invalid channel width: " << channelWidth);
    }

  // Enable logging
  LogComponentEnable("WifiPhyTest", LOG_LEVEL_INFO);

  WifiPhyTest test;

  // Run simulation with given parameters
  test.Run(distance, simulationTime, mcsValue, channelWidth, useShortGuardInterval,
           useSpectrumModel, useTcp, pcapEnabled);

  return 0;
}