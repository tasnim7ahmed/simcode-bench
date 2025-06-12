#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer uavNodes;
  uavNodes.Create(3);

  NodeContainer gcsNode;
  gcsNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(uavNodes);
  allNodes.Add(gcsNode);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingProtocol(aodv);
  internet.Install(allNodes);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("uav-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer uavDevices = wifi.Install(phy, mac, uavNodes);
  NetDeviceContainer gcsDevice = wifi.Install(phy, mac, gcsNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer uavInterfaces = address.Assign(uavDevices);
  Ipv4InterfaceContainer gcsInterface = address.Assign(gcsDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  MobilityHelper mobilityUAV;
  mobilityUAV.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Z", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=50.0]"));
  mobilityUAV.SetMobilityModel("ns3::RandomWalk3dMobilityModel",
                               "Mode", StringValue("Time"),
                               "Time", StringValue("1s"),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobilityUAV.Install(uavNodes);

  MobilityHelper mobilityGCS;
  mobilityGCS.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityGCS.Install(gcsNode);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(gcsNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(60.0));

  UdpEchoClientHelper echoClient(gcsInterface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(uavNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(60.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(60.0));

  AnimationInterface anim("uav-aodv.xml");
  anim.SetConstantPosition(gcsNode.Get(0), 50, 50, 0);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}