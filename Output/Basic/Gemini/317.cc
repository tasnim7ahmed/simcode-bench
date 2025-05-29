#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrierPhy::TxPower", DoubleValue(30));
  Config::SetDefault ("ns3::LteUeComponentCarrierPhy::TxPower", DoubleValue(23));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper.SetUeDeviceAttribute("DlEarfcn", UintegerValue(100));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);

  lteHelper.Attach(ueDevs, enbDevs.Get(0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1));
  serverApps.Stop(Seconds(10));

  UdpClientHelper client(enbIpIface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2));
  clientApps.Stop(Seconds(10));

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}