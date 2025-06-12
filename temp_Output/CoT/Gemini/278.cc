#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(500));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18000));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper(epcHelper);

  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  ueMobility.Install(ueNodes);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
  Ipv4InterfaceContainer enbIpIface;
  enbIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(enbLteDevs));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 80;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(enbIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}