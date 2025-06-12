#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::LteUePhy::EnableUeMeasurements", BooleanValue(false));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");

  Point2D enbPosition;
  enbPosition.x = 500.0;
  enbPosition.y = 500.0;
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobMod = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMobMod->SetPosition(enbPosition);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", DoubleValue(500.0),
                                  "Y", DoubleValue(500.0),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.Install(ueNodes);

  NetDeviceContainer enbLteDevs;
  NetDeviceContainer ueLteDevs;
  NetDeviceContainer enbNetDevs;
  std::tie(enbLteDevs, ueLteDevs) = lteHelper.InstallEnbUeDevice(enbNodes, ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = internet.AssignIpv4Addresses(ueLteDevs, Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"));
  Ipv4InterfaceContainer enbIpIface;
  enbIpIface = internet.AssignIpv4Addresses(enbLteDevs, Ipv4Address("10.1.1.1"), Ipv4Mask("255.255.255.0"));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(enbIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
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