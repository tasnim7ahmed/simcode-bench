#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiApStaPacketTrace");

void
RxPacket(Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND("Received one packet of size: " << packet->GetSize() << " from: " << InetSocketAddress::ConvertFrom(address).GetIpv4());
}

int
main(int argc, char *argv[])
{
  double simulationTime = 10.0;
  CommandLine cmd;
  cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
  cmd.Parse(argc, argv);

  LogComponentEnable("WifiApStaPacketTrace", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(wifiApNode);
  allNodes.Add(wifiStaNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  // STA devices
  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // AP device
  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));   // STA1
  positionAlloc->Add(Vector(5.0, 5.0, 0.0));   // STA2
  positionAlloc->Add(Vector(0.0, 5.0, 0.0));   // STA3
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  // Install applications - UDP echo between STAs via AP

  // STA1 sends to STA2
  uint16_t port1 = 9000;
  UdpEchoServerHelper echoServer1(port1);
  ApplicationContainer serverApps1 = echoServer1.Install(wifiStaNodes.Get(1));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient1(staInterfaces.GetAddress(1), port1);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(wifiStaNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(simulationTime - 1));

  // STA2 sends to STA3
  uint16_t port2 = 9001;
  UdpEchoServerHelper echoServer2(port2);
  ApplicationContainer serverApps2 = echoServer2.Install(wifiStaNodes.Get(2));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient2(staInterfaces.GetAddress(2), port2);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(wifiStaNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(simulationTime - 1));

  // STA3 sends to STA1
  uint16_t port3 = 9002;
  UdpEchoServerHelper echoServer3(port3);
  ApplicationContainer serverApps3 = echoServer3.Install(wifiStaNodes.Get(0));
  serverApps3.Start(Seconds(1.0));
  serverApps3.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient3(staInterfaces.GetAddress(0), port3);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(wifiStaNodes.Get(2));
  clientApps3.Start(Seconds(2.0));
  clientApps3.Stop(Seconds(simulationTime - 1));

  // Enable pcap tracing
  phy.EnablePcap("wifi-ap", apDevice.Get(0));
  phy.EnablePcap("wifi-sta1", staDevices.Get(0));
  phy.EnablePcap("wifi-sta2", staDevices.Get(1));
  phy.EnablePcap("wifi-sta3", staDevices.Get(2));

  // Attach packet receive callbacks for tracing
  Ptr<NetDevice> dev0 = staDevices.Get(0);
  Ptr<NetDevice> dev1 = staDevices.Get(1);
  Ptr<NetDevice> dev2 = staDevices.Get(2);

  dev0->SetReceiveCallback(MakeCallback([] (Ptr<NetDevice>, Ptr<const Packet> packet, uint16_t, const Address &from) {
      RxPacket(packet, from);
      return true;
    }));

  dev1->SetReceiveCallback(MakeCallback([] (Ptr<NetDevice>, Ptr<const Packet> packet, uint16_t, const Address &from) {
      RxPacket(packet, from);
      return true;
    }));

  dev2->SetReceiveCallback(MakeCallback([] (Ptr<NetDevice>, Ptr<const Packet> packet, uint16_t, const Address &from) {
      RxPacket(packet, from);
      return true;
    }));

  // Measure throughput and packet loss with flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Process Flow Monitor data
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::cout << "FlowID\tSrcAddr\tDstAddr\tTxPkt\tRxPkt\tLostPkt\tThroughput(Mbps)\n";
  for (const auto &flow : stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
    double throughput = flow.second.rxBytes * 8.0 / (simulationTime - 2.0) / 1e6;
    std::cout << flow.first << "\t"
              << t.sourceAddress << "\t"
              << t.destinationAddress << "\t"
              << flow.second.txPackets << "\t"
              << flow.second.rxPackets << "\t"
              << (flow.second.txPackets - flow.second.rxPackets) << "\t"
              << throughput << std::endl;
  }
  Simulator::Destroy();
  return 0;
}