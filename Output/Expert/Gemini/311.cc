#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::DefaultTransmissionMode", IntegerValue (1));
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (30.0));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(3350));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18300));
  lteHelper.SetEnbDeviceAttribute("DlBandwidth", UintegerValue(100));
  lteHelper.SetEnbDeviceAttribute("UlBandwidth", UintegerValue(100));
  lteHelper.SetEnbDeviceAttribute("CellId", UintegerValue(1));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
  lteHelper.Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  Ipv4Address enbIpAddr = enbIpIface.GetAddress(0);
  Ipv4Address ue0IpAddr = ueIpIface.GetAddress(0);
  Ipv4Address ue1IpAddr = ueIpIface.GetAddress(1);

  UintegerValue port = 8080;

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(ue0IpAddr, port.Get()));
  ApplicationContainer sinkApp = sinkHelper.Install(ueNodes.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(ue0IpAddr, port.Get()));
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApp = sourceHelper.Install(ueNodes.Get(1));
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(10.0));

  lteHelper.EnableTraces();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  Simulator::Destroy();

  return 0;
}