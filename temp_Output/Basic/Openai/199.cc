#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterLanSimulation");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("ThreeRouterLanSimulation", LOG_LEVEL_INFO);

  // Parameters
  uint32_t nLanNodes = 4; // LAN nodes (excluding R2 which bridges)
  double simTime = 10.0;

  // Create nodes
  Ptr<Node> r1 = CreateObject<Node> ();
  Ptr<Node> r2 = CreateObject<Node> ();
  Ptr<Node> r3 = CreateObject<Node> ();
  NodeContainer routers (r1, r2, r3);

  NodeContainer lanNodes;
  lanNodes.Create (nLanNodes);

  // LAN includes R2 interface + LAN hosts
  NodeContainer lanAllNodes;
  lanAllNodes.Add (r2);
  for (uint32_t i = 0; i < nLanNodes; ++i) {
    lanAllNodes.Add (lanNodes.Get(i));
  }

  // Point-to-Point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d1 = p2p.Install (r1, r2); // R1-R2
  NetDeviceContainer d2 = p2p.Install (r2, r3); // R2-R3

  // Csma LAN for R2 and LAN nodes
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer lanDevices = csma.Install (lanAllNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (routers);
  stack.Install (lanNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r1r2 = ipv4.Assign (d1);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r2r3 = ipv4.Assign (d2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_lan = ipv4.Assign (lanDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Choose sender and receiver
  Ptr<Node> lanSender = lanNodes.Get (0);
  Ptr<Node> dstNode = r3;

  // Receiver app (sink) on R3
  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (if_r2r3.GetAddress (1), sinkPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (dstNode);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  // Sender app (BulkSend) on LAN node
  BulkSendHelper sender ("ns3::TcpSocketFactory", sinkAddress);
  sender.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  ApplicationContainer senderApp = sender.Install (lanSender);
  senderApp.Start (Seconds (1.0));
  senderApp.Stop (Seconds (simTime));

  // Enable FlowMonitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Output statistics to file
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::ofstream outFile;
  outFile.open ("simulation-output.txt");

  outFile << "FlowID,SrcAddr,DstAddr,TxPackets,RxPackets,LostPackets,Throughput(bps)" << std::endl;
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      double throughput = flow.second.rxBytes * 8.0 / (simTime - 1.0); // ignore first second for app start
      outFile << flow.first << ","
              << t.sourceAddress << "," << t.destinationAddress << ","
              << flow.second.txPackets << "," << flow.second.rxPackets << ","
              << (flow.second.txPackets - flow.second.rxPackets) << ","
              << throughput << std::endl;
    }
  outFile.close ();

  Simulator::Destroy ();
  return 0;
}