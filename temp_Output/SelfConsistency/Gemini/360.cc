#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpEcho");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableDlTxLogging", BooleanValue (false));
  Config::SetDefault ("ns3::LteEnbPhy::EnableUlRxLogging", BooleanValue (false));
  Config::SetDefault ("ns3::LteEnbRrc::EpsBearerToRlcMapping", StringValue ("explicit"));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Set some LTE parameters (optional)
  lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(50));

  // Install LTE devices to the nodes
  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install the IP stack on the nodes
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses to the devices
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  // Attach the UE to the eNB
  lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

  // Set active protocol
  enum EpsBearer::Qci qci = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer(qci);
  lteHelper->ActivateDedicatedEpsBearer(ueDevs, bearer, enbDevs.Get(0));

  // Create UDP echo server on eNB
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create UDP echo client on UE
  UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}