#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer meshNodes;
  meshNodes.Create(4);

  MeshHelper mesh;
  mesh.SetStandard(WIFI_PHY_STANDARD_80211s);
  mesh.SetSpanningTreeProtocol(MeshHelper::SpanningTreeProtocol::RLearning);
  mesh.SetMacType(MeshHelper::DEFAULT);

  NetDeviceContainer meshDevices = mesh.Install(meshNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes);

  InternetStackHelper internet;
  internet.Install(meshNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(14.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(13.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(15.0));

  for (uint32_t i = 0; i < meshDevices.GetN(); ++i) {
    Simulator::Schedule(Seconds(0.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTable, meshNodes.Get(i));
  }

  for (uint32_t i = 0; i < meshDevices.GetN(); ++i) {
    Ptr<Node> node = meshNodes.Get(i);
    std::stringstream ss;
    ss << "mesh-node-" << i << ".pcap";
    Ptr<NetDevice> nd = meshDevices.Get(i);
    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile(ss.str(), std::ios::out, PcapHelper::DLT_IEEE802_11_RADIO);
    nd->TraceConnectWithoutContext("MonitorSnifferRx", MakeBoundCallback(&PcapFileWrapper::Write, file));

  }

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}