#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteSimplexUdpExample");

int main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  double interPacketInterval = 1.0;

  // Create 1 eNB and 1 UE
  NodeContainer ueNodes;
  NodeContainer enbNodes;
  ueNodes.Create (1);
  enbNodes.Create (1);

  // Create EPC helper, LTE helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Install Mobility
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityEnb.Install (enbNodes);

  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel (
      "ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
      "Distance", DoubleValue (10.0),
      "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"));
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (50.0, 50.0, 0.0));
  mobilityUe.SetPositionAllocator (positionAlloc);
  mobilityUe.Install (ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP to UEs, and get their addresses
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Attach UE to eNB
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Get the address of the remote host (in EPC)
  Ptr<Node> remoteHost;
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  remoteHost = remoteHostContainer.Get (0);

  // setup remotehost (no need to connect, just for UDP server)
  InternetStackHelper internetHost;
  internetHost.Install (remoteHostContainer);

  // Connect PGW/SGW to remote host (epc automatically creates p2pNetDevices for PGW <-> remote host)
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  // Set default route for remoteHost
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Set default route for UE
  Ptr<Node> ueNode = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // UDP Server on "eNB" side => actually on EPC remote host
  uint16_t serverPort = 49999;
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApps = server.Install (remoteHost);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Client on UE node, sends to server IP at remoteHost
  UdpClientHelper client (internetIpIfaces.GetAddress (1), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}