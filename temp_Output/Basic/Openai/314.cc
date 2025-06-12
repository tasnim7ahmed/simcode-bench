#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  NodeContainer ueNodes;
  ueNodes.Create (1);
  NodeContainer enbNodes;
  enbNodes.Create (1);

  // Install Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  mobility.Install (ueNodes);

  // Install LTE Devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP addresses to UEs, attaches to EPC
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Set default gateway in the UE
  Ptr<Node> ueNode = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // Install UDP server on the UE
  uint16_t udpPort = 9;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP client on remote host (use a node connected to PGW)
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);

  // Connect remote host to PGW
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  // Install Internet stack on remote host
  internet.Install (remoteHost);

  // Assign IP to remoteHost
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer remoteHostIface = ipv4h.Assign (internetDevices);

  // Set up routing between remoteHost and UEs
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Install UDP client on remoteHost to send to UE
  UdpClientHelper udpClient (ueIpIface.GetAddress (0), udpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (remoteHost);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}