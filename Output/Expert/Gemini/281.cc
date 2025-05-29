#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetEnbAntennaModelAttribute("Orientation", DoubleValue(0));
  lteHelper.SetEnbAntennaModelAttribute("Beamwidth", DoubleValue(90));

  Point2D enbPosition(25.0, 25.0);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator("ns3::ListPositionAllocator");
  enbMobility.Install(enbNodes);
  Ptr<ListPositionAllocator> enbPositionAlloc = DynamicCast<ListPositionAllocator>(enbMobility.GetPositionAllocator());
  enbPositionAlloc->Add(enbPosition);

  RandomWalk2dMobilityModel::Boundaries bound;
  bound.xMin = 0;
  bound.xMax = 50;
  bound.yMin = 0;
  bound.yMax = 50;
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", RectangleValue(bound));
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

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting(enbNodes.Get(0)->GetObject<Ipv4>());
  enbStaticRouting->SetDefaultRoute("10.1.1.1", 1);

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(enbIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    clientApps.Add(client.Install(ueNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}