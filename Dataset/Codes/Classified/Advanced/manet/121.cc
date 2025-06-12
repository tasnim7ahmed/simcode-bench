#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-module.h" // Include AODV header

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("AdhocNetworkExample");

int main (int argc, char *argv[])
{
   LogComponentEnable ("AdhocNetworkExample", LOG_LEVEL_INFO);
  // Basic setup
  NodeContainer centralCommand;
  centralCommand.Create (1);

  NodeContainer medicalUnits;
  medicalUnits.Create (3);

  NodeContainer teamA, teamB, teamC;
  teamA.Create (5);
  teamB.Create (5);
  teamC.Create (5);

  // Setup WiFi
  WifiHelper wifi;
  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(20));
  phy.Set("TxPowerEnd", DoubleValue(20));
  phy.Set("TxGain", DoubleValue(0.0));
  phy.Set("RxGain", DoubleValue(0.0));
  
  Ssid ssid = Ssid ("DisasterRecoveryNet");

  // Setup AP and Medical Units
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer medicalDevices = wifi.Install (phy, mac, medicalUnits);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, centralCommand.Get (0));

  // Mobility model for fixed positions
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (centralCommand);
  mobility.Install (medicalUnits);

  // Setup Ad Hoc Networks for teams
  NetDeviceContainer teamADevices, teamBDevices, teamCDevices;

  mac.SetType ("ns3::AdhocWifiMac");
  teamADevices = wifi.Install (phy, mac, teamA);
  teamBDevices = wifi.Install (phy, mac, teamB);
  teamCDevices = wifi.Install (phy, mac, teamC);

  // Mobility model for random movement
  MobilityHelper teamMobility;
  teamMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel");
  teamMobility.Install(teamA);
  teamMobility.Install(teamB);
  teamMobility.Install(teamC);

  // Internet stack and routing protocols
  InternetStackHelper stack1,stack2,stack3,stack4,stack5;
  AodvHelper aodv;
  stack1.SetRoutingHelper(aodv);
  stack2.SetRoutingHelper(aodv);
  stack3.SetRoutingHelper(aodv);
  stack4.SetRoutingHelper(aodv);
  stack5.SetRoutingHelper(aodv);
  stack1.Install(teamA);
  stack2.Install(teamB);
  stack3.Install(teamC);
  stack4.Install(medicalUnits);
  stack5.Install(centralCommand);


  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  address.Assign (medicalDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (teamADevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (teamBDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  address.Assign (teamCDevices);
  
 //enable RTS/CTS
  bool enableCtsRts = true;
  UintegerValue ctsThr = (enableCtsRts ? UintegerValue(100) : UintegerValue(2200));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);
 
  // UDP Echo Client/Server application setup for teamA, teamB, teamC
  int16_t port = 9; // well-known echo port number
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps;
  for (NodeContainer team : {teamA, teamB, teamC})
  {

    for (uint32_t i = 0; i < team.GetN (); ++i)
    {
      serverApps.Add (echoServer.Install (team.Get (i)));
    }
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (100.0));

    // Create UDP echo client application on each node
    ApplicationContainer clientApps[team.GetN ()];

    for (uint32_t i = 0; i < team.GetN (); ++i)
    {
      UdpEchoClientHelper echoClient (team.Get ((i + 1) % team.GetN ())->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (100)); // Adjust as needed
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      clientApps[i].Add (echoClient.Install (team.Get (i)));
    }

    // Start and stop client applications
    for (uint32_t i = 0; i < team.GetN (); i+4)
    {
      clientApps[i].Start (Seconds (2.0 + 10 * i));
      clientApps[i].Stop (Seconds (100.0));
    }
  } 

  // Define a flow monitor object
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(100.0));

    // Animation
    AnimationInterface anim("final.xml");

    // Run simulation
    Simulator::Run ();

    // Calculate packet delivery ratio and end-to-end delay and throughput
    uint32_t packetsDelivered = 0;
    uint32_t totalPackets = 0;
    double totalDelay = 0;

    // Retrieve the flow statistics
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for(auto & it: stats) {
        FlowId flowId = it.first;
        std::cout << "Flow " << flowId << std::endl;
        std::cout << " Tx Packets: " << it.second.txPackets << std::endl;
        std::cout << " Tx Bytes: " << it.second.txBytes << std::endl;
        std::cout << " Rx Packets: " << it.second.rxPackets << std::endl;
        std::cout << " Rx Bytes: " << it.second.rxBytes << std::endl;

        if(it.second.rxPackets > 0) {
            packetsDelivered += it.second.rxPackets;
            totalPackets += it.second.txPackets;
            totalDelay += (it.second.delaySum.ToDouble(ns3::Time::S) / it.second.rxPackets);
        }
    }

    double packetDeliveryRatio = (double) packetsDelivered / totalPackets;
    double averageEndToEndDelay = totalDelay / packetsDelivered;
    double throughput = (double) packetsDelivered * 1024 * 8 / (100.0 - 0.0) / 1000000;

    // Print the results
    std::cout << "Packets Delivered Ratio: " << packetDeliveryRatio << std::endl;
    std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << std::endl;
    std::cout << "Throughput: " << throughput << std::endl;

    Simulator::Destroy ();

    return 0;
}
