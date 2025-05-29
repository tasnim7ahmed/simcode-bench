#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(1024 * 1024));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(1.0),
                                 "DeltaY", DoubleValue(1.0), "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gnbNodes);

  Ptr<RandomWalk2dMobilityModel> randomWalk = CreateObject<RandomWalk2dMobilityModel>();
  randomWalk->SetAttribute("Bounds", RectangleValue(Rectangle(0, 0, 100, 100)));
  ueNodes.Get(0)->AddMobility(randomWalk);
  randomWalk->SetPosition(Vector(50.0, 50.0, 0.0)); // Initial position for UE
  randomWalk->Start(Seconds(0.1));

  // NR network
  NrHelper nrHelper;

  PointToPointHelper p2pBackhaul;
  p2pBackhaul.SetDeviceAttribute("DataRate", StringValue("100Gb/s"));
  p2pBackhaul.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

  NetDeviceContainer gnbNetDev = nrHelper.InstallGnb(gnbNodes);
  NetDeviceContainer ueNetDev = nrHelper.InstallUe(ueNodes);

  NetDeviceContainer backhaulNetDev = p2pBackhaul.Install(gnbNodes.Get(0), ueNodes.Get(0));

  // Internet stack
  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbNetDev);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueNetDev);
  Ipv4InterfaceContainer backhaulIpIface = ipv4.Assign(backhaulNetDev);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // TCP server on gNodeB
  uint16_t port = 5000;
  Address serverAddress(InetSocketAddress(gnbIpIface.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(gnbNodes.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // TCP client on UE
  OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(512));
  onOffHelper.SetAttribute("DataRate", StringValue("10Mb/s"));

  ApplicationContainer clientApp = onOffHelper.Install(ueNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(7.0)); // Send for 5 seconds

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}