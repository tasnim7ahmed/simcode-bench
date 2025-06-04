#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkExample");

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes for WSN (6 sensor nodes)
  NodeContainer sensorNodes;
  sensorNodes.Create(6);

  // Set up CSMA (wired communication simulation for sensor network)
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(sensorNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(sensorNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(devices);

  // Set up mobility model for sensor nodes (grid placement)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);

  // Set up UDP server on the base station (node 0)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(sensorNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Set up UDP clients on sensor nodes
  UdpEchoClientHelper echoClient(sensorInterfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < sensorNodes.GetN(); ++i)
  {
    clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable pcap tracing
  csma.EnablePcapAll("wsn-example");

  // Set up FlowMonitor for performance analysis
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

  // Set up NetAnim for visualization
  AnimationInterface anim("wsn-animation.xml");
  anim.SetConstantPosition(sensorNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(sensorNodes.Get(1), 20.0, 0.0);
  anim.SetConstantPosition(sensorNodes.Get(2), 40.0, 0.0);
  anim.SetConstantPosition(sensorNodes.Get(3), 0.0, 20.0);
  anim.SetConstantPosition(sensorNodes.Get(4), 20.0, 20.0);
  anim.SetConstantPosition(sensorNodes.Get(5), 40.0, 20.0);

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Analyze FlowMonitor statistics
  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  for (auto it = stats.begin(); it != stats.end(); ++it)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
    std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
    std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
    std::cout << "  End-to-End Delay: " << it->second.delaySum.GetSeconds() << " seconds" << std::endl;
  }

  // Clean up
  Simulator::Destroy();
  return 0;
}

