#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer wiredNodes;
  wiredNodes.Create(4);

  NodeContainer wirelessNodes;
  wirelessNodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer wiredDevices;
  for (uint32_t i = 0; i < 3; ++i) {
    NetDeviceContainer link = pointToPoint.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
    wiredDevices.Add(link.Get(0));
    wiredDevices.Add(link.Get(1));
  }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, wirelessNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, wiredNodes.Get(0));

  InternetStackHelper stack;
  stack.Install(wiredNodes);
  stack.Install(wirelessNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces;
  for (uint32_t i = 0; i < 4; ++i) {
    NetDeviceContainer ndc;
    if (i == 0) {
        ndc.Add(wiredDevices.Get(0));
    } else if (i == 3){
        ndc.Add(wiredDevices.Get(wiredDevices.GetN() - 1));
    }
    else
    {
        ndc.Add(wiredDevices.Get(i*2 -1));
        ndc.Add(wiredDevices.Get(i*2));
    }
    wiredInterfaces.Add(address.Assign(ndc));
    address.NewNetwork();
  }

  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  address.NewNetwork();

  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(wiredNodes.Get(3));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
      BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(wiredInterfaces.GetAddress(3,0), port));
      source.SetAttribute("SendSize", UintegerValue(1400));
      ApplicationContainer sourceApps = source.Install(wirelessNodes.Get(i));
      sourceApps.Start(Seconds(2.0 + i * 0.1));
      sourceApps.Stop(Seconds(9.0));
  }

  Simulator::Stop(Seconds(10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  phy.EnablePcap("hybrid-network", apDevices.Get(0));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double totalBytes = 0;
  Time firstTxTime = Seconds(100000);

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      if (t.destinationAddress == wiredInterfaces.GetAddress(3,0) && t.sourcePort == port)
      {
        totalBytes += iter->second.bytesReceived;
        if (iter->second.timeFirstTxPacket < firstTxTime)
            firstTxTime = iter->second.timeFirstTxPacket;
      }
  }

  double throughput = (totalBytes * 8.0) / (Simulator::Now().GetSeconds() - firstTxTime.GetSeconds()) / 1000000;
  std::cout << "Average throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}