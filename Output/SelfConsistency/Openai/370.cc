#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  // Enable logging for applications if needed
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install a PacketSink (receiver) on node 1
  uint16_t port = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);

  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Install a BulkSendApplication (sender) on node 0
  BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited sending

  ApplicationContainer sourceApp = bulkSend.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(10.0));

  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

      if (t.destinationPort == port)
        {
          double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
          std::cout << "Flow ID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
          std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
          std::cout << "  Throughput: " << throughput << " Mbps\n";
      }
    }

  Simulator::Destroy();
  return 0;
}