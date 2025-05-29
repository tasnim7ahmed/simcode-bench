#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter("PacketSink", LOG_LEVEL_INFO);

  // Star Topology (TCP)
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer serverDevices;
  NetDeviceContainer clientDevices;

  for (int i = 0; i < 3; ++i) {
    NodeContainer link(serverNode.Get(0), clientNodes.Get(i));
    NetDeviceContainer devices = pointToPoint.Install(link);
    serverDevices.Add(devices.Get(0));
    clientDevices.Add(devices.Get(1));
  }

  // Mesh Topology (UDP)
  NodeContainer meshNodes;
  meshNodes.Create(5);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-adhoc");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer meshDevices = wifi.Install(phy, mac, meshNodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(clientNodes);
  stack.Install(meshNodes);

  OlsrHelper olsr;
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);
  Ipv4InterfaceContainer clientInterfaces;
  for (auto device : clientDevices)
      clientInterfaces.Add(address.Assign(NetDeviceContainer(device)));
  Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  address.SetBase("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces2 = address.Assign(meshDevices);

  // TCP Application (Star Topology)
  uint16_t port = 9;
  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(serverNode.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  for (int i = 0; i < 3; ++i) {
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(serverInterfaces.GetAddress(0), port));
    source.SetAttribute("SendSize", UintegerValue(1024));
    source.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps = source.Install(clientNodes.Get(i));
    sourceApps.Start(Seconds(2.0 + i));
    sourceApps.Stop(Seconds(9.0));
  }

  // UDP Application (Mesh Topology)
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(meshInterfaces2.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Mobility (Mesh Topology)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=10]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-20, 20, -20, 20)));
  mobility.Install(meshNodes);

  // Animation
  AnimationInterface anim("hybrid-network.xml");
  anim.SetConstantPosition(serverNode.Get(0), 10, 10);
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        anim.SetConstantPosition(clientNodes.Get(i), 5+i*5, 5);
    }
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        anim.UpdateNodeSize(meshNodes.Get(i)->GetId(), 5.0, 5.0);
    }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
	  std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
  }
  monitor->SerializeToXmlFile("hybrid-network.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}