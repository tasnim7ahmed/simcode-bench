#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::MmWavePhyMacCommon::ResourceBlockNum", UintegerValue(1));
  Config::SetDefault("ns3::MmWaveSpectrumPhy::CenterFrequency", DoubleValue(28e9));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("25.0"),
                                "Y", StringValue("25.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("50|50|50|50"));
  mobility.Install(ueNodes);

  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);

  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetGnbAntennaModelType("ns3::MmWaveAntennaArrayModel");
  mmwaveHelper.SetUeAntennaModelType("ns3::MmWaveAntennaArrayModel");

  NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes);

  InternetStackHelper stack;
  stack.Install(gnbNodes);
  stack.Install(ueNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = address.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5001;

  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  Time interPacketInterval = MilliSeconds(10);
  UdpClientHelper client(ueIpIface.GetAddress(0), port);
  client.SetTotalBytes(1000000);
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  client.SetAttribute("Interval", TimeValue(interPacketInterval));
  ApplicationContainer clientApp = client.Install(ueNodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  mmwaveHelper.EnablePcap("mmwave-simulation", gnbDevs.Get(0));
  mmwaveHelper.EnablePcap("mmwave-simulation", ueDevs.Get(0));
  mmwaveHelper.EnablePcap("mmwave-simulation", ueDevs.Get(1));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}