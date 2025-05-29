#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-adhoc");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  UdpClientServerHelper echoClientServer(9);
  echoClientServer.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClientServer.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  echoClientServer.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    uint32_t dest = (i + 1) % nodes.GetN();
    echoClientServer.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(dest)));
    serverApps.Add(echoClientServer.CreateServer(interfaces.GetAddress(i), 9));
    clientApps.Add(echoClientServer.CreateClient(interfaces.GetAddress(i), 9));
    clientApps.Start(Seconds(5.0 + (double)i * 0.5));
  }

  serverApps.Start(Seconds(1.0));

  Simulator::Stop(Seconds(30.0));

  phy.EnablePcapAll("adhoc");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double sumDelays = 0;
  int numFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    totalPacketsSent += i->second.txPackets;
    totalPacketsReceived += i->second.rxPackets;
    sumDelays += i->second.delaySum.GetSeconds();
    numFlows++;
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double avgEndToEndDelay = (totalPacketsReceived > 0) ? (sumDelays / totalPacketsReceived) : 0;

  std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << avgEndToEndDelay << " seconds" << std::endl;

  monitor->SerializeToXmlFile("adhoc_flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}