#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DlEarfcn", UintegerValue (38000));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::UlEarfcn", UintegerValue (18000));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetAttribute ("PathlossModel", StringValue ("ns3::HybridBuildingsPropagationLossModel"));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    Ptr<Node> ue = ueNodes.Get(i);
    Ptr<LteUeNetDevice> lteUeDev = DynamicCast<LteUeNetDevice>(ueDevs.Get(i));
    lteHelper.Attach(lteUeDev, enbDevs.Get(0));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("2s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(ueNodes);

  Ptr<Node> ueServer = ueNodes.Get(1);
  Ptr<Node> ueClient = ueNodes.Get(0);

  uint16_t port = 8080;

  Address serverAddress (InetSocketAddress (ueIpIface.GetAddress (1, 0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (ueServer);
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (ueClient, TcpSocketFactory::GetTypeId ());
  Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket> (ns3TcpSocket);

  Ptr<MyApp> clientApp = CreateObject<MyApp> ();
  clientApp->Setup (tcpSocket, serverAddress, 1000, 1000, DataRate ("1Mbps"));
  ueClient->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (2.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}