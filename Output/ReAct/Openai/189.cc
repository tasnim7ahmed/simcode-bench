#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  uint32_t nNodes = 4;
  NodeContainer nodes;
  nodes.Create(nNodes);

  // Install CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create TCP receivers on node 0 (server)
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), sinkPort));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Create TCP traffic from nodes 1, 2, 3 to node 0
  for (uint32_t i = 1; i < nNodes; ++i)
    {
      BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
      source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
      ApplicationContainer sourceApps = source.Install(nodes.Get(i));
      sourceApps.Start(Seconds(1.0 + i*0.1));
      sourceApps.Stop(Seconds(10.0));
    }

  // Enable NetAnim
  AnimationInterface anim("csma-tcp-netanim.xml");

  // Enable pcap tracing
  csma.EnablePcapAll("csma-tcp");

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Schedule simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Metrics: packet loss, delay, throughput
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      if (t.destinationPort == sinkPort)
        {
          std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
          std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
          std::cout << "  Delay (ms): " << (it->second.delaySum.GetMilliSeconds() / (double) it->second.rxPackets) << "\n";
          std::cout << "  Throughput (Mbps): " << (it->second.rxBytes * 8.0 / (10.0 * 1000000.0)) << "\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}