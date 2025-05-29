#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create two nodes: eNB and UE
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE Helper
  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper.SetUeDeviceAttribute("UlEarfcn", UintegerValue(36100));

  // Create EPC Helper
  Ptr<LteEPC> epcHelper = CreateObject<LteEPC>();
  lteHelper.SetEpcHelper(epcHelper);

  // Create Remote Host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Connect remote host to EPC
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainerInternetStackHelper
  NetDeviceContainer dp = p2ph.Install(remoteHost, epcHelper->GetPgwNode());
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ipv4Intfs = ipv4h.Assign(dp);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  // Install the IP stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Set the default gateway for the UE
  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

  // Attach the UE to the eNB
  lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Create UDP application
  uint16_t port = 12345;
  UdpEchoServerHelper server(port);
  ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper client(ueIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  client.SetAttribute("PacketSize", UintegerValue(100);

  ApplicationContainer clientApp = client.Install(enbNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}