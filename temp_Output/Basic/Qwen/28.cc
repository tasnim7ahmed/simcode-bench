#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiNetworkPerformance");

class WifiScenarioHelper {
public:
  WifiScenarioHelper();
  void RunSimulation(bool protectionMode, bool shortPpdu, bool shortSlot, uint32_t payloadSize, bool useTcp);
};

WifiScenarioHelper::WifiScenarioHelper()
{
}

void
WifiScenarioHelper::RunSimulation(bool protectionMode, bool shortPpdu, bool shortSlot, uint32_t payloadSize, bool useTcp)
{
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  // Set ERP protection mode
  if (protectionMode) {
    wifi.Set("ProtectionMode", StringValue("RtsCts"));
  }

  // HT configurations
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(shortPpdu ? "HtMcs7" : "HtMcs0"),
                               "ControlMode", StringValue("HtMcs0"));

  Ssid ssid = Ssid("mixed-wifi-network");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MicroSeconds(102400)),
              "ShortSlotTimeSupported", BooleanValue(shortSlot));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevices);

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Traffic configuration
  uint16_t port = 9;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (useTcp) {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    serverApps = sink.Install(wifiStaNodes.Get(1));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientApps = onoff.Install(wifiStaNodes.Get(0));
  } else {
    UdpEchoServerHelper echoServer(port);
    serverApps = echoServer.Install(wifiStaNodes.Get(1));

    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientApps = echoClient.Install(wifiStaNodes.Get(0));
  }

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));
  clientApps.Stop(Seconds(10.0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  double throughput = 0;
  uint64_t totalRxBytes = 0;

  for (auto it = monitor->GetFlowStats().begin(); it != monitor->GetFlowStats().end(); ++it) {
    totalRxBytes += it->second.rxBytes;
  }

  throughput = (totalRxBytes * 8) / (10.0 * 1e6); // Mbps

  std::cout << "Config: Prot=" << protectionMode << ", ShortPPDU=" << shortPpdu
            << ", ShortSlot=" << shortSlot << ", Payload=" << payloadSize
            << ", TCP=" << useTcp << " -> Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  WifiScenarioHelper helper;

  // Run all combinations
  for (bool protection : {false, true}) {
    for (bool shortPpdu : {false, true}) {
      for (bool shortSlot : {false, true}) {
        for (uint32_t payload : {512, 1024, 1472}) {
          for (bool tcp : {false, true}) {
            helper.RunSimulation(protection, shortPpdu, shortSlot, payload, tcp);
          }
        }
      }
    }
  }

  return 0;
}