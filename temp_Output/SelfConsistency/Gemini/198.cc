#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2VWaveExample");

int main (int argc, char *argv[])
{
  // Enable logging for the Wave module
  LogComponentEnable ("V2VWaveExample", LOG_LEVEL_INFO);

  // Set simulation time
  Time::SetResolution (Time::NS);
  double simulationTime = 10.0; // seconds

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install Wave device on each node
  WaveMacHelper waveMacHelper;
  WavePhyHelper wavePhyHelper = WavePhyHelper::Default ();
  WaveHelper waveHelper = WaveHelper::Default ();

  NetDeviceContainer waveDevices = waveHelper.Install (wavePhyHelper, waveMacHelper, nodes);

  // Set mobility for the nodes (linear movement)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  // Set initial velocities
  Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  mob0->SetVelocity (Vector (10, 0, 0)); // 10 m/s in the X direction

  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mob1->SetVelocity (Vector (10, 0, 0)); // 10 m/s in the X direction

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (waveDevices);

  // Create UDP application (sender on node 0, receiver on node 1)
  uint16_t port = 9; // Discard port (RFC 863)

  UdpClientHelper client (i.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  // Create FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable PCAP tracing
  wavePhyHelper.EnablePcapAll ("v2v-wave-example");

  // Run the simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
      NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
      NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
      NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");
    }

  Simulator::Destroy ();

  return 0;
}