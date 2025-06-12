#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("LteUeUdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create Helper objects
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create EPC nodes
  Ptr<Node> pgw = epcHelper->GetPgwNode();

  // Create remote host node
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Connect remote server to pgw
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer p2pDevices = p2ph.Install(pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(p2pDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Create LTE nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach the UE to the eNodeB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Set active protocol to UDP
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer(q);
  lteHelper->ActivateDataRadioBearer(ueNodes.Get(0), bearer);

  // Install the IP stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Set the default gateway for the UE
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

  // Create UDP application on UE
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create UDP application on eNB
  uint32_t packetSize = 1024;
  Time interPacketInterval = MilliSeconds(10);
  uint32_t maxPackets = 1000;
  UdpClientHelper echoClient(ueIpIface.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApp = echoClient.Install(enbNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Cleanup
  Simulator::Destroy();
  return 0;
}