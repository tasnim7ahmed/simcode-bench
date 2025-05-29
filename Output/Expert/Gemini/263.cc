#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));

  Point2D enbPosition(0.0, 0.0);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);
  Ptr<Node> enbNode = enbNodes.Get(0);
  Ptr<ConstantPositionMobilityModel> enbMobModel = enbNode->GetObject<ConstantPositionMobilityModel>();
  enbMobModel->SetPosition(enbPosition);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-50|Max=50]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-50|Max=50]"));
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  ueMobility.Install(ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}