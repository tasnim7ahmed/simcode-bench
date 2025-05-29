#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvAnim");

int main (int argc, char *argv[])
{
  uint32_t numNodes = 4;
  double simTime = 30.0;
  double distance = 50.0;
  double areaWidth = 200.0;
  double areaHeight = 200.0;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=200.0|MaxY=200.0]"));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (10.0),
                                "MinY", DoubleValue (10.0),
                                "DeltaX", DoubleValue (distance),
                                "DeltaY", DoubleValue (distance),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel (wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Internet Stack + Routing (AODV)
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP Traffic (each node sends to next node)
  uint16_t port = 8000;
  ApplicationContainer apps;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t receiver = (i + 1) % numNodes;
      UdpServerHelper server (port);
      ApplicationContainer serverApp = server.Install (nodes.Get(receiver));
      serverApp.Start (Seconds (1.0));
      serverApp.Stop (Seconds (simTime - 1.0));

      UdpClientHelper client (interfaces.GetAddress(receiver), port);
      client.SetAttribute ("MaxPackets", UintegerValue (10000));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.05)));
      client.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer clientApp = client.Install (nodes.Get(i));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (simTime - 1.0));
      apps.Add (serverApp);
      apps.Add (clientApp);
    }

  // FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  // NetAnim
  AnimationInterface anim ("adhoc-aodv.xml");
  for (uint32_t i = 0; i < numNodes; ++i)
    anim.SetConstantPosition (nodes.Get (i), 20 + i * distance, 20);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // FlowMonitor statistics
  double totalSent = 0;
  double totalReceived = 0;
  double totalDelay = 0;
  double throughput = 0;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  uint32_t rxPackets = 0, txPackets = 0;
  for (auto& flow : stats)
    {
      txPackets += flow.second.txPackets;
      rxPackets += flow.second.rxPackets;
      totalDelay += flow.second.delaySum.GetSeconds();
      if (flow.second.rxPackets > 0)
        {
          throughput += flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1024; // kbps, traffic started at 2s
        }
    }

  double pdr = txPackets > 0 ? ((double)rxPackets / (double)txPackets) * 100.0 : 0.0;
  double avgDelay = rxPackets > 0 ? totalDelay / rxPackets : 0.0;

  std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
  std::cout << "Throughput: " << throughput << " kbps" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

  Simulator::Destroy ();
  return 0;
}