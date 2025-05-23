#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

NS_LOG_COMPONENT_DEFINE ("LteUdpEchoSimulation");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // 1. Create LTE Helper and EPC Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // 2. Create Nodes: one eNB and one UE
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // 3. Install Mobility Model (ConstantPositionMobilityModel)
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper enbMobility;
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (5.0, 0.0, 0.0)); // UE slightly away from eNB
  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator (uePositionAlloc);
  ueMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  ueMobility.Install (ueNodes);

  // 4. Install Internet Stack on UE and eNB (and later, remote host)
  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes); // eNB nodes need internet stack for EPC interfaces

  // 5. Install LTE devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // 6. Attach UE to eNB
  // This automatically handles the bearer setup and IP address assignment for the UE by the PGW
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // 7. Setup external network and application server
  // Get the PGW node from the EPC helper
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a remote host node to act as the server for the UE
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  internet.Install (remoteHostContainer); // Install InternetStack on the remote host

  // Connect the PGW to the remote host using a Point-to-Point link
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer internetDevices = p2pHelper.Install (pgw, remoteHostContainer.Get (0));

  // Assign IP addresses from the 10.0.0.0/24 range to this P2P link
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.0.0.0", "255.255.255.0"); // Use 10.0.0.0/24 range
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  
  // Get the IP address of the remote host (it's the second device in the container)
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  // Set up the UDP Echo Server on the remote host
  UdpEchoServerHelper echoServer (9); // Listen on port 9
  ApplicationContainer serverApps = echoServer.Install (remoteHostContainer.Get (0));
  serverApps.Start (Seconds (1.0)); // Server starts at 1s
  serverApps.Stop (Seconds (10.0)); // Server stops at 10s

  // Set up the UDP Echo Client on the UE
  UdpEchoClientHelper echoClient (remoteHostAddr, 9); // Send to remote host's IP on port 9
  echoClient.SetAttribute ("MaxPackets", UintegerValue (80)); // 8 seconds duration (2s to 10s) * 10 packets/sec
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1))); // 0.1s intervals
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (2.0)); // Client starts at 2s
  clientApps.Stop (Seconds (10.0)); // Client stops at 10s

  // 8. Run simulation
  Simulator::Stop (Seconds (10.0)); // Run simulation for 10s
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}