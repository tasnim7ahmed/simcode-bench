#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DlEarfcn", UintegerValue (50));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::UlEarfcn", UintegerValue (18050));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetEnbAntennaModelAttribute("Orientation", DoubleValue(0.0));
  lteHelper.SetEnbAntennaModelAttribute("Beamwidth", DoubleValue(60.0));
  lteHelper.SetEnbDeviceAttribute("DlBandwidth", UintegerValue(5));
  lteHelper.SetEnbDeviceAttribute("UlBandwidth", UintegerValue(5));

  Point2D enbPosition(0.0, 0.0);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);
  Ptr<ConstantPositionMobilityModel> enbConstantPosition = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbConstantPosition->SetPosition(Vector(enbPosition.x, enbPosition.y, 0.0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("0.1s"),
                              "Speed", StringValue("1.0ms"));
  ueMobility.Install(ueNodes);

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueDevs, enbDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);

  uint16_t dlPort = 10000;
  UdpClientHelper client(enbIpIface.GetAddress(0), dlPort);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1472));
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(dlPort);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}