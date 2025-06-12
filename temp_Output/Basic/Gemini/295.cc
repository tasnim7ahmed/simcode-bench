#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpExample");

int main(int argc, char *argv[]) {
  uint16_t numBearersPerUe = 1;

  CommandLine cmd;
  cmd.AddValue("numBearersPerUe", "Number of Bearers per UE", numBearersPerUe);
  cmd.Parse(argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(50));

  Point2D enbPosition(0.0, 0.0);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator("ns3::ConstantPositionAllocator", "ConstantPosition", PointValue(enbPosition));
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  ueMobility.Install(ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);

  for (uint16_t i = 0; i < ueNodes.GetN(); i++) {
    Ptr<Node> ueNode = ueNodes.Get(i);
    for (uint16_t j = 0; j < numBearersPerUe; j++) {
      EpsBearer bearer = EpsBearer(EpsBearer::Qci::GBR_CONV_VOICE);
      lteHelper.ActivateDedicatedBearer(ueLteDevs.Get(i), bearer, ueIpIface.GetAddress(i));
    }
  }

  uint16_t dlPort = 2000;
  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(0.1));
  dlPacketSinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  Time interPacketInterval = MilliSeconds(100);

  UdpClientHelper dlClientHelper(ueIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));
  dlClientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  dlClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer dlClientApps = dlClientHelper.Install(enbNodes.Get(0));
  dlClientApps.Start(Seconds(1));
  dlClientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  Simulator::Destroy();

  return 0;
}