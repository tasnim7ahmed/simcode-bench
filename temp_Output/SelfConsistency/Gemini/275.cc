#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUeMobility");

int main(int argc, char *argv[]) {
  // Set up tracing (optional)
  // LogComponentEnable("LteUeMobility", LOG_LEVEL_INFO);

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create eNB Node
  NodeContainer enbNodes;
  enbNodes.Create(1);

  // Create UE Nodes
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Configure LTE Helper
  lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper->SetUeDeviceAttribute("UlEarfcn", UintegerValue(18100));

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("1m/s"),
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(ueNodes);

  // Set eNB position
  Ptr<ConstantPositionMobilityModel> enbMobility =
      CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  Vector enbPosition(50, 50, 0); // Example position in the center
  enbMobility->SetPosition(enbPosition);

  // Install the IP stack on the nodes
  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  // Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);

  // Attach UE to eNB
  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  // Set active protocol.
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

  // Create UDP application:  Simplex Client on UE
  uint16_t port = 9; // well-known echo port number
  Address enbAddress(InetSocketAddress(enbIpIface.GetAddress(0), port));
  Ptr<SimplexUdpClient> client = CreateObject<SimplexUdpClient>();
  client->SetRemote(enbAddress);
  ueNodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(2.0));
  client->SetStopTime(Seconds(10.0));
  client->SetNPackets(5);
  client->SetPacketSize(512);

  // Create UDP application: Simplex Server on eNB
  Ptr<SimplexUdpServer> server = CreateObject<SimplexUdpServer>();
  enbNodes.Get(0)->AddApplication(server);
  server->SetStartTime(Seconds(1.0));
  server->SetStopTime(Seconds(10.0));
  server->SetPort(port);

  Simulator::Stop(Seconds(10));

  Simulator::Run();

  Simulator::Destroy();

  return 0;
}