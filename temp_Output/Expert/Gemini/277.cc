#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (1000000));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);

  Ptr<ConstantPositionMobilityModel> gnbPos = gnbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  gnbPos->SetPosition(Vector(0.0, 0.0, 0.0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  ueMobility.Install(ueNodes);

  NetDeviceContainer gnbDevs;
  NetDeviceContainer ueDevs;

  NrHelper nrHelper;
  gnbDevs = nrHelper.InstallGnbDevice(gnbNodes);
  ueDevs = nrHelper.InstallUeDevice(ueNodes);

  nrHelper.Attach(ueDevs, gnbDevs.Get(0));

  InternetStackHelper stack;
  stack.Install(gnbNodes);
  stack.Install(ueNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = address.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

  uint16_t port = 5000;
  Address sinkAddress (Ipv4Address::GetAny());
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (sinkAddress, port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (gnbNodes.Get(0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address (InetSocketAddress (gnbIpIface.GetAddress (0), port)));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(512));
  onOffHelper.SetAttribute("MaxBytes", UintegerValue(5 * 512));

  ApplicationContainer clientApps = onOffHelper.Install (ueNodes.Get(0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (6.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}