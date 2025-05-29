#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpExample");

int main(int argc, char *argv[]) {
  // Enable logging from the LteTcpExample component
  LogComponentEnable("LteTcpExample", LOG_LEVEL_INFO);

  // Set the simulation time
  double simTime = 10.0;

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Mobility model
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(enbPositionAlloc);
  enbMobility.Install(enbNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
  uePositionAlloc->Add(Vector(100.0, 50.0, 0.0));
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.SetPositionAllocator(uePositionAlloc);
  ueMobility.Install(ueNodes);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;
  Ipv4AddressHelper ipAddresses;
  ipAddresses.SetBase("10.1.1.0", "255.255.255.0");
  ueIpIface = ipAddresses.Assign(ueLteDevs);
  enbIpIface = ipAddresses.Assign(enbLteDevs);

  // Attach UE to eNodeB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Activate default EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer(q);
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

  // Create a TCP application
  uint16_t dlPort = 5000;
  Address enbAddress(InetSocketAddress(enbIpIface.GetAddress(0), dlPort));
  PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(0.0));
  dlPacketSinkApp.Stop(Seconds(simTime + 1));

  OnOffHelper onoffHelper("ns3::TcpSocketFactory", enbAddress);
  onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  ApplicationContainer onoffApp = onoffHelper.Install(ueNodes.Get(0));
  onoffApp.Start(Seconds(1));
  onoffApp.Stop(Seconds(simTime));

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Print per flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
    NS_LOG_UNCOND("  Tx Bytes:   " << i->second.txBytes);
    NS_LOG_UNCOND("  Rx Bytes:   " << i->second.rxBytes);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps");
  }

  Simulator::Destroy();

  return 0;
}