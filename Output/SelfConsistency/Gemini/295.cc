#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdp");

int main (int argc, char *argv[])
{
  // Set up command line arguments
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("LteUdp", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create EPC Helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create nodes: eNodeB and UE
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // Configure Mobility
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                   "X", StringValue ("0.0"),
                                   "Y", StringValue ("0.0"),
                                   "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", StringValue ("1s"),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Bounds", StringValue ("0|10|0|10"));
  ueMobility.Install (ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach the UE to the eNodeB
  lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

  // Set active protocol stack
  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);
  epcHelper->InstallEpcDevice (enbNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);

  // Install Applications (UDP server on UE, UDP client on eNodeB)
  uint16_t port = 9;  // well-known echo port number

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (ueIpIface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}