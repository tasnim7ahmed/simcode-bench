#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::NrUeNetDevice::DlEarfcn", UintegerValue(36200));
  Config::SetDefault("ns3::NrUeNetDevice::UlEarfcn", UintegerValue(36200));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("0|100|0|100"));
  ueMobility.Install(ueNodes);

  Ptr<NrGnbBaseStation> gnb = CreateObject<NrGnbBaseStation>();
  gnb->SetGnbId(1);
  gnb->SetCellId(1);

  NetDeviceContainer gnbNetDevices = gnb->Install(gnbNodes.Get(0));

  NetDeviceContainer ueNetDevices;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    Ptr<NrUeNetDevice> ue = CreateObject<NrUeNetDevice>();
    ueNetDevices.Add(ue->Install(ueNodes.Get(i), gnb));
  }

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbInterfaces = ipv4.Assign(gnbNetDevices);
  Ipv4InterfaceContainer ueInterfaces = ipv4.Assign(ueNetDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(gnbInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(gnbNodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(ueNodes.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> client = CreateObject<MyApp>();
  client->Setup(ns3TcpSocket, sinkAddress, 512, 5, Seconds(1));
  ueNodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(1.0));
  client->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}