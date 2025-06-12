#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSimulation");

void ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      // Just receive, for counting only
    }
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;
  double simTime = 100.0;
  double txpDistance = 80.0;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("txpDistance", "Transmission range", txpDistance);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0206));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0206));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-80.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-80.0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel (
      "ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=6.0]"),
      "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
      "PositionAllocator", StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=200.0|MaxY=200.0]"));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (50.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  AsciiTraceHelper ascii;
  MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("mobility-trace.tr"));

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 4000;

  // Source node 0 sends UDP to destination node 4
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (4), port));
  onoff.SetConstantRate (DataRate ("128kbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  // Sink on node 4
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (4));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  uint64_t rxPackets = 0, txPackets = 0;
  for (const auto &flow : stats)
    {
      txPackets += flow.second.txPackets;
      rxPackets += flow.second.rxPackets;
    }
  std::cout << "Packet delivery ratio: " << (rxPackets * 100.0 / (txPackets ? txPackets : 1)) << " %" << std::endl;

  Simulator::Destroy ();

  return 0;
}