#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteUePhy::EnableAmc", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Mobility
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityEnb.Install (enbNodes);

  MobilityHelper mobilityUe;
  mobilityUe.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobilityUe.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobilityUe.Install (ueNodes);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

  // Set the default gateway for the UE
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> ());
  ueStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);

  // Simplex UDP server on eNB
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Simplex UDP client on UE
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  Time interPacketInterval = Seconds (1);

  UdpClientHelper client (enbIpIface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (6.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}