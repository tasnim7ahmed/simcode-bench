#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvGridEchoSimulation");

void PrintRoutingTableAllNodes (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4RoutingHelper* routing = node->GetObject<Ipv4> ()->GetRoutingProtocol ();
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
  
  Ptr<Ipv4RoutingProtocol> aodv = ipv4->GetRoutingProtocol ();
  Ptr<Ipv4RoutingProtocol> proto_aodv = aodv;

  aodv::RoutingProtocol* aodvRoutingProtocol = dynamic_cast<aodv::RoutingProtocol*> (PeekPointer(proto_aodv));

  if (aodvRoutingProtocol)
    {
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
      aodvRoutingProtocol->PrintRoutingTable (routingStream, printTime);
    }
}

void RoutingTablePeriodicOutput (NodeContainer nodes, Time interval)
{
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Simulator::Schedule (interval * i, &PrintRoutingTableAllNodes, nodes.Get(i), Simulator::Now());
    }
  Simulator::Schedule (interval, &RoutingTablePeriodicOutput, nodes, interval);
}

int main (int argc, char *argv[])
{
  uint32_t nNodes = 10;
  double simTime = 60.0;
  double startTime = 1.0;
  double clientInterval = 4.0;
  double areaWidth = 100.0;
  double areaHeight = 100.0;
  std::string dataRate = "6Mbps";
  bool printRouting = true;
  double routingInterval = 15.0;

  CommandLine cmd;
  cmd.AddValue ("printRouting", "Print routing tables at intervals", printRouting);
  cmd.AddValue ("simTime", "Total duration of simulation", simTime);
  cmd.AddValue ("routingInterval", "Interval for printing routing tables", routingInterval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (nNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devs = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;

  uint32_t gridWidth = std::ceil(std::sqrt(nNodes));
  uint32_t gridHeight = (nNodes + gridWidth - 1) / gridWidth;
  double stepX = areaWidth / (gridWidth + 1);
  double stepY = areaHeight / (gridHeight + 1);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (stepX),
                                "MinY", DoubleValue (stepY),
                                "DeltaX", DoubleValue (stepX),
                                "DeltaY", DoubleValue (stepY),
                                "GridWidth", UintegerValue (gridWidth),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install (nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devs);

  uint16_t echoPort = 9;
  ApplicationContainer serverApps, clientApps;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      UdpEchoServerHelper echoServer (echoPort);
      ApplicationContainer servApp = echoServer.Install (nodes.Get(i));
      servApp.Start (Seconds (0.5));
      servApp.Stop (Seconds (simTime));
      serverApps.Add (servApp);
    }

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t next = (i + 1) % nNodes;
      UdpEchoClientHelper echoClient (ifaces.GetAddress (next), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simTime / clientInterval)));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (clientInterval)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (64));
      ApplicationContainer clientApp = echoClient.Install (nodes.Get(i));
      clientApp.Start (Seconds (startTime));
      clientApp.Stop (Seconds (simTime));
      clientApps.Add (clientApp);
    }

  wifiPhy.EnablePcapAll ("adhoc-aodv-grid");

  if (printRouting)
    {
      Simulator::Schedule (Seconds (routingInterval), &RoutingTablePeriodicOutput, nodes, Seconds (routingInterval));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  AnimationInterface anim ("adhoc-aodv-grid.xml");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  double totalDelay = 0.0;
  double totalThroughput = 0.0;

  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      txPackets += iter->second.txPackets;
      rxPackets += iter->second.rxPackets;
      totalDelay += (iter->second.delaySum.GetSeconds());
      double duration = iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds();
      if (duration > 0)
        {
          double throughput = iter->second.rxBytes * 8.0 / duration / 1024;
          totalThroughput += throughput;
        }
    }

  double pdr = (txPackets > 0) ? (100.0 * rxPackets / txPackets) : 0;
  double avgDelay = (rxPackets > 0) ? (totalDelay / rxPackets) : 0;
  double avgThroughput = totalThroughput / stats.size();

  std::cout << "\n--- Simulation Results ---\n";
  std::cout << "Sent Packets: " << txPackets << std::endl;
  std::cout << "Received Packets: " << rxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
  std::cout << "Average Throughput: " << avgThroughput << " Kbps" << std::endl;

  Simulator::Destroy ();

  return 0;
}