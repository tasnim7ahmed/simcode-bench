#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/nr-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableUeMeasurements", BooleanValue (false));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Set up NR network devices
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
  nrHelper->SetSchedulerType ("ns3::RrNrPfFfMacScheduler");

  NetDeviceContainer enbDevs = nrHelper->InstallGnb (enbNodes);
  NetDeviceContainer ueDevs = nrHelper->InstallUe (ueNodes);

  // Mobility
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  ueMobility.Install(ueNodes);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  // Application: TCP server on gNodeB
  uint16_t port = 5000;
  Address serverAddress(InetSocketAddress(enbIpIface.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApps = packetSinkHelper.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Application: TCP client on UE
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(512));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps = onOffHelper.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(6.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}