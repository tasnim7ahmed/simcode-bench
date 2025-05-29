#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("500"));

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(23));
  phy.Set("TxPowerEnd", DoubleValue(23));
  phy.Set("RxSensitivity", DoubleValue(-90));

  QosWaveMacHelper mac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    clientApps.Add(echoClient.Install(nodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantVelocityMobilityModel> cvmm;
  cvmm = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector(10,0,0));
  cvmm = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector(12,0,0));
  cvmm = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector(15,0,0));
  cvmm = nodes.Get(3)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector(8,0,0));
  cvmm = nodes.Get(4)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector(11,0,0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("vanet.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 10.0);
  anim.SetConstantPosition(nodes.Get(1), 5.0, 10.0);
  anim.SetConstantPosition(nodes.Get(2), 10.0, 10.0);
  anim.SetConstantPosition(nodes.Get(3), 15.0, 10.0);
  anim.SetConstantPosition(nodes.Get(4), 20.0, 10.0);

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";
    std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
  }

  Simulator::Destroy();
  return 0;
}