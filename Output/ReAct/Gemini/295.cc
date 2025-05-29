#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE Helper
  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(500));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18500));

  // Mobility
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue("Time"),
                               "Time", StringValue("0.1s"),
                               "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
  ueMobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  // Attach UE to eNB
  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  // Set active protocol stack
  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);

  // Install application
  uint16_t dlPort = 9;
  UdpServerHelper dlServer(dlPort);
  ApplicationContainer dlServerApps = dlServer.Install(ueNodes.Get(0));
  dlServerApps.Start(Seconds(1.0));
  dlServerApps.Stop(Seconds(10.0));

  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer dlClientApps = dlClient.Install(enbNodes.Get(0));
  dlClientApps.Start(Seconds(1.0));
  dlClientApps.Stop(Seconds(10.0));


  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}