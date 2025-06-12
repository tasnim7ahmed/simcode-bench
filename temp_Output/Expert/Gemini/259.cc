#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (5));
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (Seconds (1.0)));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (1024));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));

  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetGnbAntennaModelType("ns3::CosineAntennaModel");
  mmwaveHelper.SetUeAntennaModelType("ns3::CosineAntennaModel");

  NetDeviceContainer gnbDevs;
  gnbDevs = mmwaveHelper.InstallGnb(gnbNodes);

  NetDeviceContainer ueDevs;
  ueDevs = mmwaveHelper.InstallUe(ueNodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));

  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(gnbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("0 10 0 10"));
  ueMobility.Install(ueNodes);

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(ueIpIface.GetAddress(1), 9);
  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}