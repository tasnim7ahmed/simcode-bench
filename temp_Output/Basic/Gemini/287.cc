#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (1024));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  Point2D gnbPosition(25.0, 25.0);

  MobilityHelper mobilityGnb;
  mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityGnb.Install(gnbNodes);
  Ptr<ConstantPositionMobilityModel> gnbMobility = gnbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  gnbMobility->SetPosition(gnbPosition);

  MobilityHelper mobilityUe;
  mobilityUe.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("25.0"),
                                  "Y", StringValue("25.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
  mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("0|50|0|50"));
  mobilityUe.Install(ueNodes);

  NetDeviceContainer gnbDevs;
  NetDeviceContainer ueDevs;
  MmWaveHelper mmwaveHelper;

  mmwaveHelper.SetGnbAntennaModelType("ns3::CosineAntennaModel");
  mmwaveHelper.SetUeAntennaModelType("ns3::CosineAntennaModel");

  gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNodes);
  ueDevs = mmwaveHelper.InstallUeDevice(ueNodes);

  mmwaveHelper.AddX2Interface(gnbNodes);

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaceGnb = ipv4h.Assign(gnbDevs);
  Ipv4InterfaceContainer interfaceUe = ipv4h.Assign(ueDevs);

  UdpServerHelper echoServer(5001);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(interfaceUe.GetAddress(0), 5001);
  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  mmwaveHelper.EnableTraces();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}