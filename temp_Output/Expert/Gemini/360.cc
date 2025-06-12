#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", StringValue ("sf40"));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (3350));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18350));
  NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper stack;
  stack.Install(enbNodes);
  stack.Install(ueNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = address.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

  Ptr<Node> enbNode = enbNodes.Get(0);
  Ptr<Node> ueNode = ueNodes.Get(0);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNode);
  mobility.Install(ueNode);

  Ptr<ConstantPositionMobilityModel> enbMobility = enbNode->GetObject<ConstantPositionMobilityModel>();
  enbMobility->SetPosition(Vector(0, 0, 0));
  Ptr<ConstantPositionMobilityModel> ueMobility = ueNode->GetObject<ConstantPositionMobilityModel>();
  ueMobility->SetPosition(Vector(5, 0, 0));

  lteHelper.Attach(ueDevs, enbDevs.Get(0));

  uint16_t dlPort = 1000;
  UdpServerHelper dlServer(dlPort);
  ApplicationContainer dlServerApps = dlServer.Install(enbNodes.Get(0));
  dlServerApps.Start(Seconds(1.0));
  dlServerApps.Stop(Seconds(10.0));

  uint16_t ulPort = 2000;
  UdpClientHelper ulClient(enbIpIface.GetAddress(0), ulPort);
  ulClient.SetAttribute("MaxPackets", UintegerValue(500));
  ulClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  ulClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer ulClientApps = ulClient.Install(ueNodes.Get(0));
  ulClientApps.Start(Seconds(2.0));
  ulClientApps.Stop(Seconds(10.0));

  UdpServerHelper ulServer(ulPort);
  ApplicationContainer ulServerApps = ulServer.Install(enbNodes.Get(0));
  ulServerApps.Start(Seconds(1.0));
  ulServerApps.Stop(Seconds(10.0));

  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(500));
  dlClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer dlClientApps = dlClient.Install(enbNodes.Get(0));
  dlClientApps.Start(Seconds(2.0));
  dlClientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}