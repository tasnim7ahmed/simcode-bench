#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableDlTxPowerAllocation", BooleanValue (false));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(38000));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18000));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);

  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueDevs.Get(0), enbDevs.Get(0));
  lteHelper.Attach(ueDevs.Get(1), enbDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  Address serverAddress = InetSocketAddress(ueIpIface.GetAddress(0), port);

  TcpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  BulkSendHelper client("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer clientApp = client.Install(ueNodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}