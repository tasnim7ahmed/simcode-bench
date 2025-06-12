#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnb-enb-device.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer cnNodes;
  cnNodes.Create(1);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(gnbNodes);
  internet.Install(cnNodes);

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  p2pHelper.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer cnGnbNetDevices = p2pHelper.Install(cnNodes.Get(0), gnbNodes.Get(0));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer cnGnbInterfaces = ipv4.Assign(cnGnbNetDevices);

  LteHelper lteHelper;
  lteHelper.SetGnbDeviceAttribute("DlEarfcn", UintegerValue(2605));
  lteHelper.SetGnbDeviceAttribute("UlEarfcn", UintegerValue(18026));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(gnbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = internet.AssignIpv4Addresses(NetDeviceContainer(ueLteDevs));
  Ipv4InterfaceContainer enbIpIface;
  enbIpIface = internet.AssignIpv4Addresses(NetDeviceContainer(enbLteDevs));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<Node> ueNode = ueNodes.Get(0);
  Ptr<Node> gnbNode = gnbNodes.Get(0);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(ueNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueIpIface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(gnbNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}