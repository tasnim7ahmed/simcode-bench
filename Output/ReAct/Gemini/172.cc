#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("adhoc-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpClientServerHelper udp(9, 1000);
  udp.SetAttribute("MaxPackets", UintegerValue(1000));
  udp.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  udp.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t i = 0; i < 5; ++i) {
        serverApps.Add(udp.CreateServer(interfaces.GetAddress(i)));
        serverApps.Start(Seconds(1.0));

        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        clientApps.Add(udp.CreateClient(interfaces.GetAddress(i), 9));
        clientApps.Start(Seconds(rand->GetValue(2.0, 5.0)));
  }

  phy.EnablePcapAll("adhoc-aodv");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(30.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = 0;
  int numFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    totalPacketsSent += i->second.txPackets;
    totalPacketsReceived += i->second.rxPackets;
    totalDelay += i->second.delaySum.GetSeconds();
    numFlows++;
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double avgEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

  std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << avgEndToEndDelay << " seconds" << std::endl;

  Simulator::Destroy();

  return 0;
}