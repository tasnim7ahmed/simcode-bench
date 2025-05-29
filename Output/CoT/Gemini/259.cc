#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (Seconds (1.0)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (5));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (1024));


  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gnbNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
  uePositionAlloc->Add(Vector(5.0, 5.0, 0.0));
  uePositionAlloc->Add(Vector(10.0, 10.0, 0.0));

  mobility.SetPositionAllocator(uePositionAlloc);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("0 20 0 20"));
  mobility.Install(ueNodes);


  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetSchedulerType("ns3::RrMmWaveScheduler");

  NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnbNonStandalone(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes);

  mmwaveHelper.AddUeGnbConnection(ueDevs, gnbDevs);

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);

  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(ueIpIface.GetAddress(1), echoPort);

  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("mmwave-udp-echo.xml");
  anim.SetConstantPosition(gnbNodes.Get(0), 10, 10);
  anim.UpdateNodeColor(gnbNodes.Get(0), 0, 0, 255);
  anim.UpdateNodeSize(gnbNodes.Get(0)->GetId(), 20, 20);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}