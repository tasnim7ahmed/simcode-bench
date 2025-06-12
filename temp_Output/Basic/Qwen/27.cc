#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodputEvaluation");

class WifiGoodputExperiment
{
public:
  WifiGoodputExperiment();
  void Run(uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval,
           bool enableRtsCts, std::string protocol, double distance,
           double expectedThroughput, double simulationTime);

private:
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
};

WifiGoodputExperiment::WifiGoodputExperiment()
{
  wifiStaNode.Create(1);
  wifiApNode.Create(1);
}

void
WifiGoodputExperiment::Run(uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval,
                           bool enableRtsCts, std::string protocol, double distance,
                           double expectedThroughput, double simulationTime)
{
  // Create channel and setup physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Setup Wi-Fi MAC layer
  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  // Set default HT configuration
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcsValue)));

  // Configure HT options
  wifi.ConfigHtOptionsCb([&](Ptr<WifiNetDevice> device) {
    Ptr<HtConfiguration> htConfig = device->GetHtConfiguration();
    htConfig->SetShortGuardIntervalSupported(shortGuardInterval);
  });

  // Configure channel width
  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));

  if (enableRtsCts)
    {
      mac.Set("RTSThreshold", UintegerValue(1));
    }
  else
    {
      mac.Set("RTSThreshold", UintegerValue(999999));
    }

  // Setup STA and AP
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-goodput")));
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-goodput")),
              "BeaconInterval", TimeValue(Seconds(2.0)));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  // Server application on AP
  uint16_t port = 9;
  if (protocol == "udp")
    {
      UdpServerHelper server(port);
      ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(Seconds(simulationTime));

      UdpClientHelper client(apInterface.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
      client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
      client.SetAttribute("PacketSize", UintegerValue(1024));
      ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
      clientApp.Start(Seconds(0.1));
      clientApp.Stop(Seconds(simulationTime));
    }
  else if (protocol == "tcp")
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(simulationTime));

      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate(expectedThroughput * 1e6)));
      onoff.SetAttribute("PacketSize", UintegerValue(1024));
      ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));
      clientApp.Start(Seconds(0.1));
      clientApp.Stop(Seconds(simulationTime));
    }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
  uint16_t channelWidth = 20;
  uint8_t mcsValue = 0;
  bool shortGuardInterval = true;
  bool enableRtsCts = false;
  std::string protocol = "udp";
  double distance = 10.0;
  double expectedThroughput = 10.0;
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("channelWidth", "Channel width in MHz: 20 or 40", channelWidth);
  cmd.AddValue("mcsValue", "MCS value from 0 to 7", mcsValue);
  cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake (true/false)", enableRtsCts);
  cmd.AddValue("protocol", "Transport protocol (tcp/udp)", protocol);
  cmd.AddValue("distance", "Distance between station and AP (meters)", distance);
  cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
  cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NS_ABORT_MSG_IF(mcsValue > 7, "MCS value must be between 0 and 7");
  NS_ABORT_MSG_IF(channelWidth != 20 && channelWidth != 40, "Channel width must be 20 or 40 MHz");
  NS_ABORT_MSG_IF(protocol != "tcp" && protocol != "udp", "Protocol must be tcp or udp");

  WifiGoodputExperiment experiment;
  experiment.Run(channelWidth, mcsValue, shortGuardInterval, enableRtsCts,
                 protocol, distance, expectedThroughput, simulationTime);

  return 0;
}