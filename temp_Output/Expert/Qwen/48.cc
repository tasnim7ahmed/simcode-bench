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

NS_LOG_COMPONENT_DEFINE("WiFiSimulation");

class WiFiTest
{
public:
  WiFiTest();
  void Run(uint32_t mcs, uint16_t channelWidth, bool shortGuardInterval, bool useSpectrumPhy, double distance, bool useTcp, Time simulationTime);

private:
  void Setup(uint32_t mcs, uint16_t channelWidth, bool shortGuardInterval, bool useSpectrumPhy, double distance);
  void Report();
  void PhyCallback(std::string context, Ptr<const Packet> packet, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t stationId);
  void CalculateThroughput();

  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  double lastTotalRx = 0.0;
};

WiFiTest::WiFiTest()
{
  wifiStaNode.Create(1);
  wifiApNode.Create(1);
}

void WiFiTest::Setup(uint32_t mcs, uint16_t channelWidth, bool shortGuardInterval, bool useSpectrumPhy, double distance)
{
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  if (useSpectrumPhy)
    {
      phy = SpectrumWifiPhyHelper::Default();
    }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcs)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-test");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  apDevice = wifi.Install(phy, mac, wifiApNode);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardInterval", BooleanValue(shortGuardInterval));

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
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  phy.EnablePcap("wifi-simulation", apDevice.Get(0));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&WiFiTest::PhyCallback));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&WiFiTest::PhyCallback));
}

void WiFiTest::PhyCallback(std::string context, Ptr<const Packet> packet, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t stationId)
{
  NS_LOG_DEBUG(context << " Signal=" << signalNoise.signal << " Noise=" << signalNoise.noise);
}

void WiFiTest::Run(uint32_t mcs, uint16_t channelWidth, bool shortGuardInterval, bool useSpectrumPhy, double distance, bool useTcp, Time simulationTime)
{
  Setup(mcs, channelWidth, shortGuardInterval, useSpectrumPhy, distance);

  uint16_t port = 9;
  if (useTcp)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      serverApps = sink.Install(wifiStaNode.Get(0));
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(simulationTime);

      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(1472));

      clientApps = onoff.Install(wifiApNode.Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(simulationTime - Seconds(0.1));
    }
  else
    {
      UdpServerHelper server(port);
      serverApps = server.Install(wifiStaNode.Get(0));
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(simulationTime);

      UdpClientHelper client(staInterfaces.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(1472));

      clientApps = client.Install(wifiApNode.Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(simulationTime - Seconds(0.1));
    }

  monitor = flowmon.InstallAll();
  Simulator::Schedule(simulationTime, &WiFiTest::Report, this);
  Simulator::Stop(simulationTime);
  Simulator::Run();
  Simulator::Destroy();
}

void WiFiTest::CalculateThroughput()
{
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  double totalRx = 0.0;
  for (auto flow : monitor->GetFlowStats())
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
      if (t.destinationPort == 9)
        {
          totalRx += flow.second.rxBytes * 8.0 / 1e6;
        }
    }
  std::cout << "Total Throughput: " << totalRx << " Mbps" << std::endl;
}

void WiFiTest::Report()
{
  CalculateThroughput();
  for (uint32_t i = 0; i < apDevice.GetN(); ++i)
    {
      Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(apDevice.Get(i));
      if (device)
        {
          Ptr<WifiPhy> phy = device->GetPhy();
          if (phy)
            {
              auto state = phy->GetState();
              NS_LOG_UNCOND("PHY State: " << state->GetStateString());
            }
        }
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  std::vector<uint32_t> mcsValues = {0, 7, 15};
  std::vector<uint16_t> channelWidths = {20, 40};
  std::vector<bool> guardIntervals = {false, true};
  std::vector<bool> phyModels = {false, true};
  double distance = 10.0;
  Time simulationTime = Seconds(10.0);
  bool useTcp = false;

  for (auto mcs : mcsValues)
    {
      for (auto width : channelWidths)
        {
          for (auto gi : guardIntervals)
            {
              for (auto spectrum : phyModels)
                {
                  WiFiTest test;
                  test.Run(mcs, width, gi, spectrum, distance, useTcp, simulationTime);
                }
            }
        }
    }

  return 0;
}