#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenNodeSimulation");

class ThroughputChecker : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ThroughputChecker")
      .SetParent<Object>()
      .AddConstructor<ThroughputChecker>();
    return tid;
  }

  ThroughputChecker(Ptr<FlowMonitor> monitor, FlowId flowId, double expectedMinThroughput, double expectedMaxThroughput, Time checkTime)
    : m_monitor(monitor), m_flowId(flowId), m_expectedMin(expectedMinThroughput), m_expectedMax(expectedMaxThroughput)
  {
    Simulator::Schedule(checkTime, &ThroughputChecker::CheckThroughput, this);
  }

private:
  void CheckThroughput()
  {
    m_monitor->CheckForLostPackets();
    FlowMonitor::FlowStats stats = m_monitor->GetFlowStats()[m_flowId];
    if (stats.timeLastRxPacket.IsZero())
    {
      NS_LOG_ERROR("No packet received at all.");
      exit(1);
    }
    double throughput = stats.rxBytes * 8.0 / (stats.timeLastRxPacket.GetSeconds());
    NS_LOG_UNCOND("Measured throughput: " << throughput / 1e6 << " Mbps");
    if (throughput < m_expectedMin || throughput > m_expectedMax)
    {
      NS_LOG_ERROR("Throughput out of bounds! Expected [" << m_expectedMin / 1e6 << ", " << m_expectedMax / 1e6 << "] Mbps, got " << throughput / 1e6 << " Mbps");
      exit(1);
    }
    else
    {
      NS_LOG_UNCOND("Throughput within expected bounds.");
    }
  }

  Ptr<FlowMonitor> m_monitor;
  FlowId m_flowId;
  double m_expectedMin;
  double m_expectedMax;
};

int main(int argc, char *argv[])
{
  bool enableRtsCts = false;
  uint32_t maxAmpduSize = 4095; // Default for 802.11n
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double expectedMinThroughput = 0.0;
  double expectedMaxThroughput = 1e9; // default high value

  CommandLine cmd(__FILE__);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue("maxAmpduSize", "Maximum A-MPDU size in bytes", maxAmpduSize);
  cmd.AddValue("payloadSize", "Payload size per UDP packet", payloadSize);
  cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
  cmd.AddValue("expectedMinThroughput", "Minimum expected throughput in bps", expectedMinThroughput);
  cmd.AddValue("expectedMaxThroughput", "Maximum expected throughput in bps", expectedMaxThroughput);
  cmd.Parse(argc, argv);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("hidden-node-network");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(2.5)));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(maxAmpduSize));

  if (enableRtsCts)
  {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/UseRtsForBar", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue(0));
  }
  else
  {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue(UINT32_MAX));
  }

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client1(staInterfaces.GetAddress(0), 9);
  client1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client1.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client1.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApp1 = client1.Install(wifiStaNodes.Get(0));
  clientApp1.Start(Seconds(0.1));
  clientApp1.Stop(Seconds(simulationTime));

  UdpClientHelper client2(staInterfaces.GetAddress(1), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client2.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client2.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApp2 = client2.Install(wifiStaNodes.Get(1));
  clientApp2.Start(Seconds(0.1));
  clientApp2.Stop(Seconds(simulationTime));

  AsciiTraceHelper asciiTraceHelper;
  phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-hidden-node.tr"));
  phy.EnablePcapAll("wifi-hidden-node");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  ThroughputChecker checker(monitor, 1, expectedMinThroughput, expectedMaxThroughput, Seconds(simulationTime - 0.5));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  monitor->SerializeToXmlFile("wifi-hidden-node.flowmon", false, false);
  Simulator::Destroy();
  return 0;
}