#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Logging (optional for debugging)
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create nodes: 1 eNB, 1 UE
  NodeContainer ueNodes;
  ueNodes.Create(1);
  NodeContainer enbNodes;
  enbNodes.Create(1);

  // Mobility: Node 1 - eNB - constant position, Node 2 - UE - random walk
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50,50,-50,50)),
                              "Distance", DoubleValue(5.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  ueMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(10.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
  ueMobility.Install(ueNodes);

  // LTE Helper setup
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create packet gateway (PGW)
  Ptr<Node> pgw = epcHelper->GetPgwNode();

  // Install LTE Devices on eNB/UE
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install Internet Stack on UE and a remote host (to simulate Packet Gateway/Core IP)
  InternetStackHelper internet;
  internet.Install(ueNodes);

  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  InternetStackHelper internetHost;
  internetHost.Install(remoteHostContainer);

  // Create point-to-point link between PGW and remoteHost
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHostContainer.Get(0));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // Assign the remote host an address
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  // Assign IP address to UE
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  Ipv4Address ueAddr = ueIpIfaces.GetAddress(0);

  // Attach UE to eNB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Install default route in UE towards PGW
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // Applications: UDP Server on eNB, UDP Client on UE

  // On eNB: we simulate the server as a task on a remote host, which is reached via LTE+EPC
  // But per user instructions: UE sends UDP packets to eNB, meaning that the eNB acts as a server.
  // The network stack is on the eNB node, not always realistic, but possible to demonstrate communication

  // Must install internet stack on eNB, assign address
  internet.Install(enbNodes);
  // Assign an address to eNB -- via a CSMA link with PGW for eNB interface (not present in EPC model by default)
  // For direct communication over LTE, we use IP allocated by EPC to the UE, and an application on eNB

  // Start UDP server (on eNB)
  uint16_t serverPort = 50000;
  UdpServerHelper udpServer(serverPort);
  ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(0.1));
  serverApps.Stop(Seconds(10.0));

  // Install static routing on eNB to direct packets back to UE (for reply if needed)
  Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting (enbNodes.Get (0)->GetObject<Ipv4> ());
  enbStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1); // For traffic to UEs, route via LTE interface

  // Get eNB interface address (assigned dynamically, we just pick the first available)
  Ptr<Ipv4> enbIpv4 = enbNodes.Get(0)->GetObject<Ipv4>();
  Ipv4Address enbAddr = enbIpv4->GetAddress(1,0).GetLocal();

  // Start UDP client on UE (send to eNB's IP)
  UdpClientHelper udpClient(enbAddr, serverPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(320));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(0.03125)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}