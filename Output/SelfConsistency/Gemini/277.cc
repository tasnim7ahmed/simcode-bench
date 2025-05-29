#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrSimpleExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("NrSimpleExample", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer gnbNodes;
  gnbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // Configure mobility
  Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator> ();
  gnbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  gnbMobility.SetPositionAllocator (gnbPositionAlloc);
  gnbMobility.Install (gnbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", EnumValue (RandomWalk2dMobilityModel::MODE_TIME),
                               "Time", StringValue ("2s"),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50))); // 100x100 area
  ueMobility.Install (ueNodes);

  // Install NR devices
  NetDeviceContainer gnbDevs;
  NetDeviceContainer ueDevs;
  NrHelper nrHelper;
  gnbDevs = nrHelper.InstallGnbDevice (gnbNodes);
  ueDevs = nrHelper.InstallUeDevice (ueNodes);

  // Install the internet stack
  InternetStackHelper internet;
  internet.Install (gnbNodes);
  internet.Install (ueNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign (gnbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueDevs);

  // Create a TCP server on the gNodeB
  uint16_t port = 5000;
  Address serverAddress (InetSocketAddress (gnbIpIface.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApps = packetSinkHelper.Install (gnbNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Create a TCP client on the UE to send packets to the gNodeB
  OnOffHelper onOffHelper ("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetConstantRate (DataRate ("1Mb/s")); // arbitrary rate
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (512));
  onOffHelper.SetAttribute ("MaxBytes", UintegerValue (5 * 512)); // Send 5 packets

  ApplicationContainer clientApps = onOffHelper.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0)); // Start sending after 1 second
  clientApps.Stop (Seconds (6.0));   // Stop after sending 5 packets (at 1-second intervals)

  // Set up simulation end time
  Simulator::Stop (Seconds (10.0));

  // Run the simulation
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}