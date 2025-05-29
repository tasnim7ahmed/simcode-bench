#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetSimulation");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue ("EnableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", TimeValue (Seconds (1.0)));

  NodeContainer nodes;
  nodes.Create (4);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("100x100"));
  mobility.Install (nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  UdpClientHelper client (interfaces.GetAddress (3), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("manet.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (nodes.Get (1), 20.0, 20.0);
  anim.SetConstantPosition (nodes.Get (2), 30.0, 30.0);
  anim.SetConstantPosition (nodes.Get (3), 40.0, 40.0);

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
    {
      flowmon.InstallAll ();
    }

  Simulator::Run ();

  if (enableFlowMonitor)
    {
      Ptr<FlowMonitor> monitor = flowmon.GetMonitor ();
      monitor->CheckForLostPackets ();

      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          std::cout << "Flow ID: " << i->first << std::endl;
          std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
          std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
          std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps" << std::endl;
        }
    }

  Simulator::Destroy ();

  return 0;
}