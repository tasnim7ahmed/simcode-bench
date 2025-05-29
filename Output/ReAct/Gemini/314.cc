#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer epcNodes;
  epcNodes.Create(1);

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  Ptr<Node> epcNode = epcNodes.Get(0);
  Ptr<Node> enbNode = enbNodes.Get(0);
  Ptr<Node> ueNode = ueNodes.Get(0);

  // EPC
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  p2pHelper.SetQueueAttribute("MaxPackets", UintegerValue(1000000));
  NetDeviceContainer epcNetDevices;
  epcNetDevices = p2pHelper.Install(epcNode, enbNode);

  InternetStackHelper internet;
  internet.Install(epcNodes);
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer epcIpIface;
  epcIpIface = ipv4.Assign(epcNetDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // LTE
  LteHelper lteHelper;
  lteHelper.SetEpcHelper(CreateObject<EpcHelper>());

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  // Applications
  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(enbNode);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}