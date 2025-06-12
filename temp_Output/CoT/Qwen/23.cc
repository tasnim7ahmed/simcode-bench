#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputTest");

class Wifi7ThroughputTest {
public:
  Wifi7ThroughputTest();
  void Run(uint8_t mcs, uint16_t channelWidth, bool shortGi, double frequency, std::string trafficType,
           uint32_t payloadSize, uint32_t mpduBufferSize, bool enableUplinkOfdma, bool enableBsrp,
           uint32_t nStations, Time simulationTime, double expectedMinThpt, double expectedMaxThpt);

private:
  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  uint32_t totalRxBytes;
  void ReceivePacket(Ptr<const Packet> packet, const Address &from);
};

Wifi7ThroughputTest::Wifi7ThroughputTest()
{
  totalRxBytes = 0;
}

void
Wifi7ThroughputTest::ReceivePacket(Ptr<const Packet> packet, const Address &from)
{
  totalRxBytes += packet->GetSize();
}

void
Wifi7ThroughputTest::Run(uint8_t mcs, uint16_t channelWidth, bool shortGi, double frequency,
                         std::string trafficType, uint32_t payloadSize, uint32_t mpduBufferSize,
                         bool enableUplinkOfdma, bool enableBsrp, uint32_t nStations,
                         Time simulationTime, double expectedMinThpt, double expectedMaxThpt)
{
  // Create nodes
  wifiStaNodes.Create(nStations);
  wifiApNode.Create(1);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  // Create and configure Wi-Fi helper
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
  wifiPhy.Set("Frequency", DoubleValue(frequency));
  wifiPhy.Set("ShortGuardInterval", BooleanValue(shortGi));
  wifiPhy.Set("MpduBufferSize", UintegerValue(mpduBufferSize));
  wifiPhy.Set("EnableUplinkOfdma", BooleanValue(enableUplinkOfdma));
  wifiPhy.Set("EnableBsrp", BooleanValue(enableBsrp));

  if (frequency == 2400e6 || frequency == 5000e6 || frequency == 6000e6)
  {
    wifiPhy.SetStandard(WIFI_STANDARD_80211be);
  }
  else
  {
    NS_FATAL_ERROR("Invalid frequency selected.");
  }

  Ssid ssid = Ssid("wifi-80211be");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  staDevices = wifiPhy.Install(wifiMac, wifiStaNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

  apDevice = wifiPhy.Install(wifiMac, wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  // Applications
  uint16_t port = 9;
  for (uint32_t i = 0; i < nStations; ++i)
  {
    if (trafficType == "udp")
    {
      UdpClientHelper udpClient(apInterface.GetAddress(0), port);
      udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
      udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

      ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
      clientApp.Start(Seconds(0.0));
      clientApp.Stop(simulationTime - Seconds(0.1));

      UdpServerHelper server(port);
      ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(simulationTime - Seconds(0.1));
    }
    else if (trafficType == "tcp")
    {
      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
      onoff.SetConstantRate(DataRate("100Mbps"));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      ApplicationContainer clientApp = onoff.Install(wifiStaNodes.Get(i));
      clientApp.Start(Seconds(0.0));
      clientApp.Stop(simulationTime - Seconds(0.1));

      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer serverApp = sink.Install(wifiApNode.Get(0));
      serverApp.Start(Seconds(0.0));
      serverApp.Stop(simulationTime - Seconds(0.1));
    }
    else
    {
      NS_FATAL_ERROR("Unknown traffic type: " << trafficType);
    }
  }

  // Set MCS
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode",
              StringValue("HeMcs" + std::to_string(mcs)));

  // Tracing
  Config::ConnectWithoutContext("/NodeList/" + std::to_string(wifiApNode.Get(0)->GetId()) +
                                 "/ApplicationList/0/$ns3::PacketSink/Rx",
                                 MakeCallback(&Wifi7ThroughputTest::ReceivePacket, this));

  Simulator::Stop(simulationTime);
  Simulator::Run();

  double throughput = (totalRxBytes * 8) / (simulationTime.GetSeconds() * 1000000);
  std::cout.precision(2);
  std::cout << "MCS: " << static_cast<uint32_t>(mcs)
            << " | Channel Width: " << channelWidth << " MHz"
            << " | GI: " << (shortGi ? "Short" : "Long")
            << " | Throughput: " << std::fixed << throughput << " Mbps" << std::endl;

  if (throughput < expectedMinThpt || throughput > expectedMaxThpt)
  {
    NS_LOG_ERROR("Throughput out of expected range! Measured: " << throughput << " Mbps");
    NS_FATAL_ERROR("Throughput validation failed for MCS=" << static_cast<uint32_t>(mcs)
                                                          << ", ChannelWidth=" << channelWidth
                                                          << ", GI=" << (shortGi ? "Short" : "Long"));
  }

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  uint8_t mcs = 11;
  uint16_t channelWidth = 80;
  bool shortGi = true;
  double frequency = 5000e6;
  std::string trafficType = "udp";
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 64;
  bool enableUplinkOfdma = true;
  bool enableBsrp = true;
  uint32_t nStations = 4;
  Time simulationTime = Seconds(10);
  double expectedMinThpt = 500;
  double expectedMaxThpt = 1200;

  CommandLine cmd(__FILE__);
  cmd.AddValue("mcs", "Modulation and Coding Scheme value", mcs);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGi", "Use Short Guard Interval", shortGi);
  cmd.AddValue("frequency", "Operating frequency in Hz", frequency);
  cmd.AddValue("trafficType", "Traffic type: tcp or udp", trafficType);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
  cmd.AddValue("enableUplinkOfdma", "Enable uplink OFDMA", enableUplinkOfdma);
  cmd.AddValue("enableBsrp", "Enable BSRP", enableBsrp);
  cmd.AddValue("nStations", "Number of stations", nStations);
  cmd.AddValue("simTime", "Simulation time", simulationTime);
  cmd.AddValue("expectedMinThpt", "Expected minimum throughput in Mbps", expectedMinThpt);
  cmd.AddValue("expectedMaxThpt", "Expected maximum throughput in Mbps", expectedMaxThpt);
  cmd.Parse(argc, argv);

  Wifi7ThroughputTest test;
  test.Run(mcs, channelWidth, shortGi, frequency, trafficType, payloadSize, mpduBufferSize,
           enableUplinkOfdma, enableBsrp, nStations, simulationTime, expectedMinThpt,
           expectedMaxThpt);

  return 0;
}