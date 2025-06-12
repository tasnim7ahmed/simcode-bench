#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteSingleEnbTwoUesUdpExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create remote nodes: 2 UEs and 1 eNodeB
  NodeContainer ueNodes;
  ueNodes.Create (2);

  NodeContainer enbNodes;
  enbNodes.Create (1);

  // EPC
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Mobility - grid placement
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (50.0),
                                "DeltaY", DoubleValue (50.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  mobility.Install (ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP address to UEs, and set up routing through EPC
  Ipv4InterfaceContainer ueIpIfs;
  ueIpIfs = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Attach UEs to eNodeB
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      lteHelper->Attach (ueLteDevs.Get (i), enbLteDevs.Get (0));
    }

  // Install and start applications
  uint16_t serverPort = 9;
  // UDP server on UE1 (second UE)
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApps = udpServer.Install (ueNodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP client on UE0 (first UE) sending to UE1
  UdpClientHelper udpClient (ueIpIfs.GetAddress (1), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (2));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = udpClient.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable default EPC default route on UEs
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      Ptr<Node> ueNode = ueNodes.Get (i);
      Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}