/* disaster_recovery_network.cc */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DisasterRecoveryNetwork");

int main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t nMedicalUnits = 3;
  uint32_t nTeams = 3;
  uint32_t respondersPerTeam = 5;
  uint32_t nCommand = 1;
  uint32_t simTime = 60; // seconds

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Node containers
  NodeContainer commandNode;
  commandNode.Create (nCommand);

  NodeContainer medicalUnits;
  medicalUnits.Create (nMedicalUnits);

  std::vector<NodeContainer> teamNodes (nTeams);
  for (uint32_t t = 0; t < nTeams; ++t)
    {
      teamNodes[t].Create (respondersPerTeam);
    }

  // Aggregate all nodes for device and mobility installation
  NodeContainer allNodes;
  allNodes.Add (commandNode);
  allNodes.Add (medicalUnits);
  for (uint32_t t = 0; t < nTeams; ++t)
    {
      allNodes.Add (teamNodes[t]);
    }

  // Wifi configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (0));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Enable RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // Mobility: Command and medical units are stationary, teams are mobile
  MobilityHelper mobilityStationary, mobilityMobile;

  // Stationary nodes: Fixed positions
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 0.0)); // Central command at origin
  for (uint32_t i = 0; i < nMedicalUnits; ++i)
    {
      posAlloc->Add (Vector (20.0 * (i+1), 0.0, 0.0));
    }
  mobilityStationary.SetPositionAllocator (posAlloc);
  mobilityStationary.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  NodeContainer stationaryNodes;
  stationaryNodes.Add (commandNode);
  stationaryNodes.Add (medicalUnits);
  mobilityStationary.Install (stationaryNodes);

  // Mobile nodes: RandomWaypointMobilityModel within a 250x250 area
  mobilityMobile.SetPositionAllocator (
      "ns3::RandomRectanglePositionAllocator",
      "X", StringValue ("Uniform(0.0, 250.0)"),
      "Y", StringValue ("Uniform(0.0, 250.0)"));
  mobilityMobile.SetMobilityModel (
      "ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("Uniform(1.0, 5.0)"),
      "Pause", StringValue ("Constant:1.0"),
      "PositionAllocator", PointerValue (mobilityMobile.GetPositionAllocator ()));

  for (uint32_t t = 0; t < nTeams; ++t)
    {
      mobilityMobile.Install (teamNodes[t]);
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Application: Each team runs a UDP echo server on the first node of the team
  uint16_t basePort = 9000;
  ApplicationContainer serverApps, clientApps;
  for (uint32_t t = 0; t < nTeams; ++t)
    {
      Ptr<Node> serverNode = teamNodes[t].Get (0);
      // UdpEchoServerHelper arguments: port number
      UdpEchoServerHelper echoServer (basePort + t);
      serverApps.Add (echoServer.Install (serverNode));
    }

  // Each team client: For each other node in the team (excluding server), send echo packets to team's server node
  for (uint32_t t = 0; t < nTeams; ++t)
    {
      Ipv4Address serverAddr = interfaces.GetAddress (
        commandNode.GetN () + medicalUnits.GetN () + t * respondersPerTeam, 0); // offset in allNodes

      for (uint32_t i = 1; i < respondersPerTeam; ++i)
        {
          Ptr<Node> clientNode = teamNodes[t].Get (i);
          UdpEchoClientHelper echoClient (serverAddr, basePort + t);
          echoClient.SetAttribute ("MaxPackets", UintegerValue (30));
          echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
          echoClient.SetAttribute ("PacketSize", UintegerValue (64));
          ApplicationContainer clientApp = echoClient.Install (clientNode);
          clientApps.Add (clientApp);
        }
    }

  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime));

  // Flow monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  // NetAnim
  AnimationInterface anim ("disaster_recovery.xml");
  // Optionally, set node descriptions
  anim.UpdateNodeDescription (commandNode.Get (0), "Command Center");
  for (uint32_t i = 0; i < nMedicalUnits; ++i)
    {
      anim.UpdateNodeDescription (medicalUnits.Get (i), "Medical Unit " + std::to_string (i+1));
    }
  // Teams
  uint32_t nodeOffset = nCommand + nMedicalUnits;
  for (uint32_t t = 0; t < nTeams; ++t)
    {
      for (uint32_t i = 0; i < respondersPerTeam; ++i)
        {
          Ptr<Node> n = allNodes.Get (nodeOffset + t * respondersPerTeam + i);
          anim.UpdateNodeDescription (n, "Team-" + std::to_string (t+1) + " Node-" + std::to_string (i+1));
        }
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Calculate and output flow monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t totalTxPackets = 0, totalRxPackets = 0;
  double totalDelay = 0.0; // nanoseconds
  double totalThroughput = 0.0;

  uint32_t activeFlows = 0;

  std::cout << "\nFlow Monitor Results:\n";
  for (const auto& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow ID: " << flow.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
      std::cout << "  Throughput: "
                << flow.second.rxBytes * 8.0 /
                     (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds () + 0.0001) / 1e3
                << " Kbps" << std::endl;
      if (flow.second.rxPackets > 0)
        {
          ++activeFlows;
          totalTxPackets += flow.second.txPackets;
          totalRxPackets += flow.second.rxPackets;
          totalDelay += (flow.second.delaySum.GetNanoSeconds ());
          totalThroughput += flow.second.rxBytes * 8.0 /
            (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds () + 0.0001) / 1e3;
        }
    }
  if (activeFlows > 0 && totalRxPackets > 0)
    {
      double avgPDR = 100.0 * ((double)totalRxPackets / (double)totalTxPackets);
      double avgDelay = totalDelay / totalRxPackets / 1e6; // ms
      double avgThroughput = totalThroughput / activeFlows; // Kbps
      std::cout << "\n=== Aggregate Statistics ===\n";
      std::cout << "Packet Delivery Ratio: " << avgPDR << " %\n";
      std::cout << "Average End-to-End Delay: " << avgDelay << " ms\n";
      std::cout << "Average Throughput: " << avgThroughput << " Kbps\n";
    }
  else
    {
      std::cout << "\nNo received packets/flws to compute aggregate statistics.\n";
    }

  Simulator::Destroy ();
  return 0;
}