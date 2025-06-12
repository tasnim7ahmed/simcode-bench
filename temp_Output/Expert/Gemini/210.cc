#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/tcp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridNetworkSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer starNodes;
  starNodes.Create (6);
  NodeContainer meshNodes;
  meshNodes.Create (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  InternetStackHelper internet;
  internet.Install (starNodes);
  internet.Install (meshNodes);

  NetDeviceContainer starDevices;
  for (uint32_t i = 1; i < starNodes.GetN (); ++i)
    {
      NetDeviceContainer link = p2p.Install (starNodes.Get (0), starNodes.Get (i));
      starDevices.Add (link.Get (0));
      starDevices.Add (link.Get (1));
    }

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces = ipv4.Assign (starDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 8080;
  TcpClientServerHelper tcpClientServerHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress (starInterfaces.GetAddress (0), sinkPort)));
  tcpClientServerHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  tcpClientServerHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  tcpClientServerHelper.SetAttribute ("Remote", AddressValue(InetSocketAddress(starInterfaces.GetAddress (0), sinkPort)));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < starNodes.GetN (); ++i)
    {
      clientApps.Add (tcpClientServerHelper.Install (starNodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  ApplicationContainer serverApp = tcpClientServerHelper.Install (starNodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));


  NetDeviceContainer meshDevices;
  for (uint32_t i = 0; i < meshNodes.GetN (); ++i)
    {
      for (uint32_t j = i + 1; j < meshNodes.GetN (); ++j)
        {
          NetDeviceContainer link = p2p.Install (meshNodes.Get (i), meshNodes.Get (j));
          meshDevices.Add (link.Get (0));
          meshDevices.Add (link.Get (1));
        }
    }

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = ipv4.Assign (meshDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpClientServerHelper udpClientServerHelper (port);
  udpClientServerHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  udpClientServerHelper.SetAttribute ("Interval", TimeValue (Time ("0.01")));
  udpClientServerHelper.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer udpClientApps, udpServerApps;

  for (uint32_t i = 0; i < meshNodes.GetN (); ++i)
    {
      for (uint32_t j = 0; j < meshNodes.GetN (); ++j) {
          if (i != j) {
            UdpClientServerHelper thisClientServerHelper(port);
            thisClientServerHelper.SetAttribute ("RemoteAddress", AddressValue(InetSocketAddress (meshInterfaces.GetAddress (j+ (j >=i ? 0:1) ), port)));
            thisClientServerHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
            thisClientServerHelper.SetAttribute ("Interval", TimeValue (Time ("0.01")));
            thisClientServerHelper.SetAttribute ("PacketSize", UintegerValue (1024));
            udpClientApps.Add (thisClientServerHelper.Install (meshNodes.Get (i)));
          }
        }
    }

    for(uint32_t i = 0; i < meshNodes.GetN (); ++i) {
        PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
        ApplicationContainer sinkApps = sink.Install (meshNodes.Get (i));
        sinkApps.Start (Seconds (0.0));
        sinkApps.Stop (Seconds (10.0));

    }
  udpClientApps.Start (Seconds (1.0));
  udpClientApps.Stop (Seconds (10.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (starNodes);
  mobility.Install (meshNodes);


  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  AnimationInterface anim ("hybrid-network.xml");
  anim.EnablePacketMetadata (true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("hybrid-network.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}