#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::LteUePhy::EnableUeMeasurements", BooleanValue(false));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Create 5G NR network devices
  NetDeviceContainer gnbNrDevs;
  NetDeviceContainer ueNrDevs;
  NrHelper nrHelper;

  gnbNrDevs = nrHelper.InstallGnbDevice(gnbNodes);
  ueNrDevs = nrHelper.InstallUeDevice(ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(ueNodes);

  Ptr<ConstantPositionMobilityModel> gnbMobility =
      CreateObject<ConstantPositionMobilityModel>();
  gnbNodes.Get(0)->AggregateObject(gnbMobility);
  Vector3D position(25.0, 25.0, 0.0);
  gnbMobility->SetPosition(position);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = address.Assign(gnbNrDevs);
  Ipv4InterfaceContainer ueIpIface = address.Assign(ueNrDevs);

  // Set remote host address on the UE
  Ptr<Node> remoteHost = gnbNodes.Get(0);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostRouting =
      ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), 1);

  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    Ptr<Node> ue = ueNodes.Get(i);
    Ptr<Ipv4StaticRouting> ueRouting =
        ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), 1);
  }

  // Create a server endpoint on the second UE
  uint16_t port = 8080;
  Address serverAddress(InetSocketAddress(ueIpIface.GetAddress(1), port));
  TcpServerHelper serverHelper(port);
  ApplicationContainer serverApps = serverHelper.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create a client endpoint on the first UE
  TcpClientHelper clientHelper;
  clientHelper.SetRemote(serverAddress);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}