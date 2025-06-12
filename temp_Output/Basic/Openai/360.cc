#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);
  lteHelper->SetAttribute("UseIdealRrc", BooleanValue(true)); // Faster simulation

  NodeContainer ueNodes;
  ueNodes.Create(1);
  NodeContainer enbNodes;
  enbNodes.Create(1);

  // Install Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // eNB
  positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // UE
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(ueNodes);

  // Assign IP to UEs, connect to remote host via EPC
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Attach UEs to eNB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Set default gateway for UE
  Ipv4Address remoteIp = ueIpIface.GetAddress(0);

  // Get SGW/PGW node and assign IP on EPC
  Ptr<Node> pgw = epcHelper->GetPgwNode();

  // Create a remote host (the server will run on eNB; need IP stack there)
  // Install internet on eNB node
  internet.Install(enbNodes);
  // Allow IP connectivity between eNB and UE by assigning IPs
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = address.Assign(enbLteDevs);
  Ipv4Address enbAddress = enbIpIface.GetAddress(0);

  // Set routing
  // Add a route on the UE for packets to eNB subnet
  Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
  ueStaticRouting->AddNetworkRouteTo(enbAddress, "255.255.255.0", 1);

  // UDP Echo Server on eNB
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo Client on UE -> eNB
  UdpEchoClientHelper echoClient(enbAddress, echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}