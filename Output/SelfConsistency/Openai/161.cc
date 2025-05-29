#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpUdpComparisonExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("TcpUdpComparisonExample", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point connection attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Application parameters
  uint16_t tcpPort = 50000;
  uint16_t udpPort = 40000;
  double appStartTime = 1.0;
  double appStopTime = 9.0;
  uint32_t maxPacketCount = 320; // For UDP only
  uint32_t packetSize = 1024; // bytes
  double dataRateUdp = 8.0; // Mbps
  // TCP will be controlled by underlying congestion control

  // ---- TCP APPLICATIONS ----
  // TCP Sink on node1
  Address tcpSinkAddress (InetSocketAddress (interfaces.GetAddress (1), tcpPort));
  PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory", tcpSinkAddress);
  ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (nodes.Get (1));
  tcpSinkApp.Start (Seconds (0.0));
  tcpSinkApp.Stop (Seconds (appStopTime + 1));

  // TCP Source on node0
  OnOffHelper tcpSource ("ns3::TcpSocketFactory", tcpSinkAddress);
  tcpSource.SetAttribute ("DataRate", DataRateValue (DataRate ("8Mbps")));
  tcpSource.SetAttribute ("PacketSize", UintegerValue (packetSize));
  tcpSource.SetAttribute ("StartTime", TimeValue (Seconds (appStartTime)));
  tcpSource.SetAttribute ("StopTime", TimeValue (Seconds (appStopTime)));
  ApplicationContainer tcpSourceApp = tcpSource.Install (nodes.Get (0));

  // ---- UDP APPLICATIONS ----
  // UDP Sink on node1
  Address udpSinkAddress (InetSocketAddress (interfaces.GetAddress (1), udpPort));
  PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory", udpSinkAddress);
  ApplicationContainer udpSinkApp = udpSinkHelper.Install (nodes.Get (1));
  udpSinkApp.Start (Seconds (0.0));
  udpSinkApp.Stop (Seconds (appStopTime + 1));

  // UDP Source on node0
  OnOffHelper udpSource ("ns3::UdpSocketFactory", udpSinkAddress);
  udpSource.SetAttribute ("DataRate", DataRateValue (DataRate (std::to_string (dataRateUdp) + "Mbps")));
  udpSource.SetAttribute ("PacketSize", UintegerValue (packetSize));
  udpSource.SetAttribute ("StartTime", TimeValue (Seconds (appStartTime)));
  udpSource.SetAttribute ("StopTime", TimeValue (Seconds (appStopTime)));
  ApplicationContainer udpSourceApp = udpSource.Install (nodes.Get (0));

  // NetAnim to visualize
  AnimationInterface anim ("tcp-udp-comparison.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 20.0);
  anim.SetConstantPosition (nodes.Get (1), 50.0, 20.0);

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Calculate and print performance metrics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  double tcpThroughput = 0;
  double udpThroughput = 0;
  double tcpDelay = 0;
  double udpDelay = 0;
  double tcpPacketDeliveryRatio = 0;
  double udpPacketDeliveryRatio = 0;
  uint64_t tcpRxPackets = 0, tcpTxPackets = 0, tcpRxBytes = 0;
  uint64_t udpRxPackets = 0, udpTxPackets = 0, udpRxBytes = 0;
  double tcpLastDelay = 0, udpLastDelay = 0;

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);

      bool isTcp = (t.destinationPort == tcpPort);
      bool isUdp = (t.destinationPort == udpPort);

      if (isTcp || isUdp)
        {
          // Throughput (in Mbps)
          double throughput = ((flow.second.rxBytes * 8.0) / (appStopTime - appStartTime)) / 1e6;

          // Average delay
          double avgDelay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) : 0.0;

          // Packet delivery ratio
          double pdr = (flow.second.txPackets > 0) ? (double(flow.second.rxPackets) / flow.second.txPackets) * 100 : 0;

          if (isTcp)
            {
              tcpThroughput += throughput;
              tcpDelay += avgDelay;
              tcpPacketDeliveryRatio += pdr;
              tcpRxPackets += flow.second.rxPackets;
              tcpTxPackets += flow.second.txPackets;
              tcpRxBytes += flow.second.rxBytes;
              tcpLastDelay = avgDelay;
            }
          if (isUdp)
            {
              udpThroughput += throughput;
              udpDelay += avgDelay;
              udpPacketDeliveryRatio += pdr;
              udpRxPackets += flow.second.rxPackets;
              udpTxPackets += flow.second.txPackets;
              udpRxBytes += flow.second.rxBytes;
              udpLastDelay = avgDelay;
            }
        }
    }

  std::cout << "========= TCP vs UDP Performance Metrics =========" << std::endl;
  std::cout << std::fixed << std::setprecision (4);

  std::cout << "TCP Throughput            : " << tcpThroughput << " Mbps" << std::endl;
  std::cout << "UDP Throughput            : " << udpThroughput << " Mbps" << std::endl;

  std::cout << "TCP Avg. End-to-End Delay : " << tcpLastDelay * 1000 << " ms" << std::endl;
  std::cout << "UDP Avg. End-to-End Delay : " << udpLastDelay * 1000 << " ms" << std::endl;

  std::cout << "TCP Packet Delivery Ratio : " << tcpPacketDeliveryRatio << " %" << std::endl;
  std::cout << "UDP Packet Delivery Ratio : " << udpPacketDeliveryRatio << " %" << std::endl;

  std::cout << "TCP Total RX Packets      : " << tcpRxPackets << std::endl;
  std::cout << "UDP Total RX Packets      : " << udpRxPackets << std::endl;

  std::cout << "TCP Total TX Packets      : " << tcpTxPackets << std::endl;
  std::cout << "UDP Total TX Packets      : " << udpTxPackets << std::endl;

  Simulator::Destroy ();
  return 0;
}