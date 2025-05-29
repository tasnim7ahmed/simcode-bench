#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/netanim-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  double simulationTime = 10; //seconds

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer sensorNodes;
  sensorNodes.Create (5);

  NodeContainer allNodes;
  allNodes.Add (sinkNode);
  allNodes.Add (sensorNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_802154);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer sinkDevice = wifi.Install (wifiPhy, wifiMac, sinkNode);
  NetDeviceContainer sensorDevices = wifi.Install (wifiPhy, wifiMac, sensorNodes);

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign (sinkDevice);
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (sinkInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i) {
      clientApps.Add (echoClient.Install (sensorNodes.Get (i)));
  }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  AnimationInterface anim ("sensor-network.xml");
  anim.SetConstantPosition (sinkNode.Get (0), 10, 10);
  for(uint32_t i=0; i < sensorNodes.GetN (); ++i){
    anim.SetConstantPosition (sensorNodes.Get (i), 15 + i*2, 10);
  }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  if (tracing)
    {
      wifiPhy.EnablePcapAll ("sensor-network");
    }

  Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
	  NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
	  NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
	  NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");
	  NS_LOG_UNCOND("  Delay: " << i->second.delaySum);
      }

  Simulator::Destroy ();
  return 0;
}