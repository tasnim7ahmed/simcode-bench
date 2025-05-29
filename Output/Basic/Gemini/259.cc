#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/config-store-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWaveSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t numberOfUes = 2;
  double simulationTime = 10.0;
  std::string phyMode("Oqpsk");
  double frequency = 28e9;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("simulationTime", "Simulation runtime in seconds", simulationTime);
  cmd.AddValue("phyMode", "PHY mode", phyMode);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(100)));
  Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(5));

  if (verbose) {
    LogComponentEnable("MmWaveSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("ns3::NrGnbPhy", LOG_LEVEL_ALL);
    LogComponentEnable("ns3::NrUePhy", LOG_LEVEL_ALL);
  }

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(numberOfUes);

  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);
  Ptr<ConstantPositionMobilityModel> gnbMobMod = gnbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  gnbMobMod->SetPosition(Vector(0.0, 0.0, 0.0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-10.0, -10.0, 10.0, 10.0)));
  ueMobility.Install(ueNodes);

  NetDeviceContainer gnbDevs;
  NetDeviceContainer ueDevs;
  NrHelper nrHelper;
  nrHelper.SetGnbAntennaModelType("ns3::CosineAntennaModel");
  nrHelper.SetUeAntennaModelType("ns3::CosineAntennaModel");
  gnbDevs = nrHelper.InstallGnbDevice(gnbNodes);
  ueDevs = nrHelper.InstallUeDevice(ueNodes);

  nrHelper.AddUeGnbLinks(ueDevs, gnbDevs);

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetGnbIface = ipv4h.Assign(gnbDevs);
  Ipv4InterfaceContainer internetUeIface = ipv4h.Assign(ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpClientHelper echoClient(internetUeIface.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 1));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}