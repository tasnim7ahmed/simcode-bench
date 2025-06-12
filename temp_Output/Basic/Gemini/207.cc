#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpAodvSimulation");

int main(int argc, char *argv[]) {
  bool enableNetAnim = true;
  std::string animFile = "wifi-tcp-aodv.xml";

  CommandLine cmd;
  cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue("animFile", "NetAnim animation file", animFile);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol("ns3::AodvRoutingProtocol",
                            "AllowedHelloLoss", UintegerValue(2));
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(apDevices);
  interfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  TcpEchoServerHelper echoServer(port);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  TcpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enableNetAnim) {
    AnimationInterface anim(animFile);
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
  }

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalBytes = 0.0;
  Time firstTxTime;
  Time lastRxTime;
  bool first = true;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.sourceAddress == interfaces.GetAddress(1) && t.destinationAddress == interfaces.GetAddress(0)) {
      totalBytes = i->second.txBytes;
      firstTxTime = i->second.timeFirstTxPacket;
      lastRxTime = i->second.timeLastRxPacket;
    }
  }

  Time duration = lastRxTime - firstTxTime;
  double throughput = (totalBytes * 8.0) / duration.GetSeconds() / 1000000;

  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}