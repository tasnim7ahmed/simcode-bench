#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::UlEarfcn", UintegerValue (18100));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Create LTE devices
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach UEs to the eNB
  lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));
  lteHelper->Attach(ueDevs.Get(1), enbDevs.Get(0));

  // Install the IP stack on the nodes
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  // Set default gateway for the UE
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  Address serverAddress = InetSocketAddress(ueIpIface.GetAddress(0), port);

  // Create the TCP echo server application on UE 0
  TcpServerHelper echoServerHelper(port);
  ApplicationContainer serverApp = echoServerHelper.Install(ueNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Create the TCP echo client application on UE 1
  TcpClientHelper echoClientHelper(ueIpIface.GetAddress(0), port);
  echoClientHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer clientApp = echoClientHelper.Install(ueNodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}