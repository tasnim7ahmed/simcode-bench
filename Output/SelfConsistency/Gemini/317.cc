#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpExample");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create EPC Helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create Remote Host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("0ms"));
  NetDeviceContainer internetDevices = p2p.Install(epcHelper->Get ব্যবস্থাপকNode(), remoteHost);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4.Assign(internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create UE and eNB Nodes
  NodeContainer ueNodes;
  NodeContainer enbNodes;
  ueNodes.Create(1);
  enbNodes.Create(1);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach UE to eNB
  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  // Set UE mobility
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.Install(ueNodes);

  // Set eNB mobility
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  // Install internet stack on the UE
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4Address ueAddr = ueIpIface.GetAddress(0);

  // Install Applications (UDP server on remote host, and UDP client on UE)
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(remoteHost);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(remoteHostAddr, port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(ueNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort);
    NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
    NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps");
  }

  Simulator::Destroy();

  return 0;
}