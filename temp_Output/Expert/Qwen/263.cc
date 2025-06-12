#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimpleSimulation");

class SimulationHelper {
public:
  static void SendPackets(Ptr<Socket> socket, Ipv4Address ueIp) {
    for (uint32_t i = 0; i < 10; ++i) {
      Ptr<Packet> packet = Create<Packet>(512);
      socket->SendTo(packet, 0, InetSocketAddress(ueIp, 9));
    }
  }
};

int main(int argc, char *argv[]) {
  Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  NodeContainer enbNode;
  enbNode.Create(1);
  NodeContainer ueNode;
  ueNode.Create(1);

  MobilityHelper mobilityEnb;
  mobilityEnb.Install(enbNode);

  MobilityHelper mobilityUe;
  mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
  mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobilityUe.Install(ueNode);

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper->InstallEnbDevice(enbNode);

  NetDeviceContainer ueDevs;
  ueDevs = lteHelper->InstallUeDevice(ueNode);

  lteHelper->Attach(ueDevs, enbDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNode);
  internet.Install(enbNode);

  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIfaces;
  enbIpIfaces = ipv4.Assign(enbDevs);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ueNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  Ptr<Socket> clientSocket = Socket::CreateSocket(enbNode.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
  Simulator::Schedule(Seconds(1.0), &SimulationHelper::SendPackets, clientSocket, ueIpIfaces.GetAddress(0));

  lteHelper->EnableTraces();
  epcHelper->EnableMmeTraces();
  epcHelper->EnableSgwTraces();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}