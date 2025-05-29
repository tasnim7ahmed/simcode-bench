#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/animation-interface.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridTcpUdpTopology");

const uint32_t nStarClients = 4;
const uint32_t nMeshNodes = 5;

void
CalculateThroughput (Ptr<PacketSink> sink, std::string name)
{
  static uint64_t lastTotalRx = 0;
  uint64_t cur = sink->GetTotalRx ();
  double throughput = (cur - lastTotalRx) * (8.0/1e6); // Mbps
  lastTotalRx = cur;
  Simulator::Schedule (Seconds (1.0), &CalculateThroughput, sink, name);
  std::cout << Simulator::Now ().GetSeconds () << "s: " << name
            << " throughput: " << throughput << " Mbps" << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // ------- STAR Topology (TCP) -------
  NodeContainer starServerNode;
  starServerNode.Create (1);
  NodeContainer starClientNodes;
  starClientNodes.Create (nStarClients);
  NodeContainer allStarNodes;
  allStarNodes.Add (starServerNode);
  allStarNodes.Add (starClientNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer starDevices[nStarClients];
  NetDeviceContainer serverDevices;
  for (uint32_t i = 0; i < nStarClients; ++i)
    {
      NodeContainer link (starServerNode.Get(0), starClientNodes.Get(i));
      NetDeviceContainer nd = p2p.Install (link);
      serverDevices.Add (nd.Get(0));
      starDevices[i].Add (nd.Get(1));
    }

  // ------- MESH Topology (UDP) -------
  NodeContainer meshNodes;
  meshNodes.Create (nMeshNodes);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, meshNodes);

  // ------- Install Internet stack -------
  InternetStackHelper stack;
  stack.Install (allStarNodes);
  stack.Install (meshNodes);

  // ------- Addressing -------
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> starIfs;

  for (uint32_t i = 0; i < nStarClients; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i+1) << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      NetDeviceContainer dc;
      dc.Add (serverDevices.Get(i));
      dc.Add (starDevices[i].Get(0));
      starIfs.push_back (address.Assign (dc));
    }

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = address.Assign (meshDevices);

  // ------- Mobility for star (fixed) -------
  Ptr<ListPositionAllocator> starPos = CreateObject<ListPositionAllocator> ();
  starPos->Add (Vector (50, 50, 0));
  double radius = 40.0;
  for (uint32_t i = 0; i < nStarClients; ++i)
    {
      double angle = 2 * M_PI * i / nStarClients;
      starPos->Add (Vector (50 + radius * std::cos (angle), 50 + radius * std::sin (angle), 0));
    }

  MobilityHelper mobStar;
  mobStar.SetPositionAllocator (starPos);
  mobStar.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobStar.Install (allStarNodes);

  // ------- Mobility for mesh (random, ad-hoc) -------
  MobilityHelper mobMesh;
  mobMesh.SetPositionAllocator ("ns3::GridPositionAllocator",
                      "MinX", DoubleValue (200.0),
                      "MinY", DoubleValue (100.0),
                      "DeltaX", DoubleValue (20.0),
                      "DeltaY", DoubleValue (20.0),
                      "GridWidth", UintegerValue (3),
                      "LayoutType", StringValue ("RowFirst"));
  mobMesh.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue (Rectangle (180, 250, 80, 160)));
  mobMesh.Install (meshNodes);

  // ------- TCP Applications (Star) -------
  uint16_t tcpPort = 50000;
  ApplicationContainer tcpApps;
  Ptr<PacketSink> tcpSink;
  for (uint32_t i = 0; i < nStarClients; ++i)
    {
      Address sinkAddr (InetSocketAddress (starIfs[i].GetAddress (0), tcpPort+i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddr);
      ApplicationContainer sinkApp = sinkHelper.Install (starServerNode.Get(0));
      sinkApp.Start (Seconds (1.0));
      sinkApp.Stop (Seconds (20.0));
      if (i==0)
        tcpSink = DynamicCast<PacketSink> (sinkApp.Get (0));

      OnOffHelper client ("ns3::TcpSocketFactory", sinkAddr);
      client.SetAttribute ("DataRate", DataRateValue (DataRate ("2Mbps")));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApp = client.Install (starClientNodes.Get(i));
      clientApp.Start (Seconds (2.0 + i));
      clientApp.Stop (Seconds (20.0));
      tcpApps.Add (clientApp);
    }

  // ------- UDP Applications (Mesh) -------
  uint16_t udpBasePort = 60000;
  uint32_t packetSize = 512;
  double udpSendRate = 1.0;
  std::vector<Ptr<PacketSink>> udpSinks;
  for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
      // Each node will have a sink listening
      Address sinkAddr (InetSocketAddress (meshInterfaces.GetAddress (i), udpBasePort+i));
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddr);
      ApplicationContainer sinkApp = sinkHelper.Install (meshNodes.Get(i));
      sinkApp.Start (Seconds (1.0));
      sinkApp.Stop (Seconds (20.0));
      udpSinks.push_back (DynamicCast<PacketSink> (sinkApp.Get (0)));
    }
  ApplicationContainer udpApps;
  for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
      for (uint32_t j = 0; j < nMeshNodes; ++j)
        {
          if (i==j) continue;
          Address remoteAddr (InetSocketAddress (meshInterfaces.GetAddress (j), udpBasePort+j));
          OnOffHelper client ("ns3::UdpSocketFactory", remoteAddr);
          client.SetAttribute ("DataRate", DataRateValue ("1Mbps"));
          client.SetAttribute ("PacketSize", UintegerValue (packetSize));
          // Only install from i->j (not both ways at same time)
          ApplicationContainer app = client.Install (meshNodes.Get(i));
          app.Start (Seconds (3.0 + i*0.5 + j*0.1));
          app.Stop (Seconds (20.0));
          udpApps.Add (app);
        }
    }

  // ------- Flow Monitor -------
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  // ------- Throughput statistics -------
  Simulator::Schedule (Seconds (2.1), &CalculateThroughput, tcpSink, "TCP-Star");

  // UDP Mesh Throughput
  for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
      std::ostringstream oss;
      oss << "UDP-Mesh-" << i;
      Simulator::Schedule (Seconds (4.1 + i), &CalculateThroughput, udpSinks[i], oss.str());
    }

  // ------- NetAnim -------
  AnimationInterface anim ("hybrid-topo.xml");

  // Custom icons/labels
  anim.UpdateNodeDescription (starServerNode.Get (0), "Star-Server");
  anim.UpdateNodeColor (starServerNode.Get (0), 0, 255, 0);
  for (uint32_t i = 0; i < nStarClients; ++i)
    {
      anim.UpdateNodeDescription (starClientNodes.Get (i), "Star-Client");
      anim.UpdateNodeColor (starClientNodes.Get (i), 0, 0, 255);
    }
  for (uint32_t i = 0; i < nMeshNodes; ++i)
    {
      anim.UpdateNodeDescription (meshNodes.Get (i), "Mesh-"+std::to_string(i));
      anim.UpdateNodeColor (meshNodes.Get (i), 255, 0, 0);
    }

  anim.EnablePacketMetadata (); // Required for packet animation
 
  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();

  // ------- Results -------
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "Flow " << iter->first << " (" << t.protocol << ") "
         << t.sourceAddress << " --> " << t.destinationAddress << std::endl;
      std::cout << "  Tx Bytes:   " << iter->second.txBytes << std::endl;
      std::cout << "  Rx Bytes:   " << iter->second.rxBytes << std::endl;
      std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
      double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()) / 1024/1024;
      std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
      if (iter->second.rxPackets > 0)
        std::cout << "  Mean Delay: " << (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) * 1000 << " ms" << std::endl;
      std::cout << "------------------" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}