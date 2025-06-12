#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(160));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(50));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(50 + 18000));

  Point2D enbPosition(50.0, 50.0);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.Install(enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobMod = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMobMod->SetPosition(enbPosition);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  ueMobility.Install(ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  uint16_t dlPort = 9;
  UdpServerHelper dlServer(dlPort);
  ApplicationContainer dlServerApps = dlServer.Install(enbNodes.Get(0));
  dlServerApps.Start(Seconds(0.0));
  dlServerApps.Stop(Seconds(10.0));

  uint32_t packetSize = 512;
  Time interPacketInterval = Seconds(1);
  uint32_t maxPackets = 5;
  UdpClientHelper dlClient(enbIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  dlClient.SetAttribute("Interval", TimeValue(interPacketInterval));
  dlClient.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer dlClientApps = dlClient.Install(ueNodes.Get(0));
  dlClientApps.Start(Seconds(1.0));
  dlClientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}