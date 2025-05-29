#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (50));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (900));

  PointToPointHelper pointToPointHelper;
  pointToPointHelper.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  pointToPointHelper.SetChannelAttribute ("Delay", StringValue ("0ms"));

  NetDeviceContainer enbNetDev = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueNetDev = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbNetDev);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueNetDev);

  lteHelper.Attach(ueNetDev.Get(0), enbNetDev.Get(0));

  // Set mobility
  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject (enbMobility);
  Vector enbPosition(0.0, 0.0, 0.0);
  enbMobility->SetPosition(enbPosition);

  Ptr<ConstantPositionMobilityModel> ueMobility = CreateObject<ConstantPositionMobilityModel>();
  ueNodes.Get(0)->AggregateObject (ueMobility);
  Vector uePosition(5.0, 0.0, 0.0);
  ueMobility->SetPosition(uePosition);

  uint16_t dlPort = 12345;

  UdpServerHelper dlServer(dlPort);
  ApplicationContainer dlServerApps = dlServer.Install(ueNodes.Get(0));
  dlServerApps.Start(Seconds(0.0));
  dlServerApps.Stop(Seconds(2.0));

  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer dlClientApps = dlClient.Install(enbNodes.Get(0));
  dlClientApps.Start(Seconds(0.1));
  dlClientApps.Stop(Seconds(2.0));

  Simulator::Stop(Seconds(2.0));

  Simulator::Run();

  Simulator::Destroy();

  return 0;
}