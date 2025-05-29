#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpExample");

int
main(int argc, char *argv[])
{
  // Enable logging from the LteUdpExample component
  LogComponentEnable("LteUdpExample", LOG_LEVEL_INFO);

  // Set the simulation start and stop times
  Time::SetResolution(Time::NS);
  Time simulationTime = Seconds(10);

  // Set up command line arguments
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Set default values for propagation model
  lteHelper->SetAttribute("PathlossModel", StringValue("ns3::LogDistancePropagationLossModel"));
  lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(3);

  // Configure mobility for eNodeB and UE nodes
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.Install(ueNodes);

  // Set eNodeB position
  Ptr<MobilityModel> enbMobModel = enbNodes.Get(0)->GetObject<MobilityModel>();
  enbMobModel->SetPosition(Vector(0.0, 0.0, 0.0));

  // Set UE positions
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
  {
    Ptr<MobilityModel> ueMobModel = ueNodes.Get(i)->GetObject<MobilityModel>();
    ueMobModel->SetPosition(Vector(10.0 + i * 5, 10.0 + i * 5, 0.0));
  }

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach UEs to the closest eNodeB
  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  // Activate default EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses to the devices interfaces
  Ipv4InterfaceContainer enbIpIface = internet.AssignIpv4Addresses(enbLteDevs, Ipv4Address("10.1.1.1"), Ipv4Mask("255.255.255.0"));
  Ipv4InterfaceContainer ueIpIface = internet.AssignIpv4Addresses(ueLteDevs, Ipv4Address("10.1.1.2"), Ipv4Mask("255.255.255.0"));

  // Create UDP server on eNodeB
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1));
  serverApps.Stop(simulationTime);

  // Create UDP client on UEs
  UdpClientHelper client(enbIpIface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
  {
    clientApps.Add(client.Install(ueNodes.Get(i)));
  }
  clientApps.Start(Seconds(2));
  clientApps.Stop(simulationTime);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(simulationTime);
  Simulator::Run();

  // Print per flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
    NS_LOG_UNCOND("  Tx Bytes:   " << i->second.txBytes);
    NS_LOG_UNCOND("  Rx Bytes:   " << i->second.rxBytes);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps");
  }

  monitor->SerializeToXmlFile("lte-udp-example.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}