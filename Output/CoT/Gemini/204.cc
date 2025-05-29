#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN");

int main (int argc, char *argv[])
{
  bool enableNetAnim = true;
  double simulationTime = 10.0;
  uint32_t packetSize = 1024;
  std::string dataRate = "1Mbps";
  std::string delay = "2ms";
  uint16_t port = 9;
  uint32_t numNodes = 6;

  CommandLine cmd;
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue ("dataRate", "Data rate of CSMA channel", dataRate);
  cmd.AddValue ("delay", "Delay of CSMA channel", delay);
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (numNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue (dataRate));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer csmaDevices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (csmaDevices);

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (0)); // Node 0 is the base station
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i)
  {
      clientApps.Add (client.Install (nodes.Get (i)));
  }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  if (enableNetAnim)
  {
    AnimationInterface anim ("wsn-netanim.xml");
    anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
    anim.SetConstantPosition (nodes.Get (1), 20.0, 20.0);
    anim.SetConstantPosition (nodes.Get (2), 30.0, 20.0);
    anim.SetConstantPosition (nodes.Get (3), 20.0, 30.0);
    anim.SetConstantPosition (nodes.Get (4), 30.0, 30.0);
    anim.SetConstantPosition (nodes.Get (5), 40.0, 30.0);
  }

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Iprobe> iprobe = monitor->GetIprobe ();
  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelaySum = 0;
  double totalBytesReceived = 0;

  for (FlowIdTag::FlowId flowId = 1; flowId <= iprobe->GetMaxFlowId(); ++flowId)
  {
    const FlowProbe::FlowCounters counters = iprobe->GetFlowCounters(flowId);

    totalPacketsSent += counters.packetsSent;
    totalPacketsReceived += counters.packetsReceived;
    totalDelaySum += counters.delaySum.GetSeconds();
    totalBytesReceived += counters.bytesReceived;
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double averageEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelaySum / totalPacketsReceived) : 0;
  double throughput = (totalBytesReceived * 8.0) / (simulationTime * 1000000.0);

  std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << " seconds" << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();

  return 0;
}