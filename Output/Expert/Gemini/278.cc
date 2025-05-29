#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue (18100));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetEnbDeviceAttribute("TxPower", DoubleValue(30));
  lteHelper.SetUeDeviceAttribute("TxPower", DoubleValue(23));

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2pHelper.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  ueMobility.Install(ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4InterfaceContainer enbIpIface;
  enbIpIface = internet.AssignIpv4Addresses(enbLteDevs, Ipv4Address("10.1.1.1"), Ipv4Mask("255.255.255.0"));
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = internet.AssignIpv4Addresses(ueLteDevs, Ipv4Address("10.1.1.2"), Ipv4Mask("255.255.255.0"));

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  UdpServerHelper server(80);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  HttpHelper client;
  client.SetAttribute("RemoteAddress", Ipv4AddressValue(enbIpIface.GetAddress(0)));
  client.SetAttribute("RemotePort", UintegerValue(80));
  client.SetAttribute("NumPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024););
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}