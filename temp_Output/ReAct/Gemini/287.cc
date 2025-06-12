#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (1024));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("25.0"),
                                "Y", StringValue("25.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(ueNodes);

  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);

  // Devices
  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetGnbAntennaModel ("ns3::CosineAntennaModel");
  mmwaveHelper.SetUeAntennaModel ("ns3::CosineAntennaModel");
  NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnb(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper.InstallUe(ueNodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(gnbNodes);
  stack.Install(ueNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = address.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications
  uint16_t port = 5001;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueIpIface.GetAddress(0), port);
  ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // PCAP Tracing
  mmwaveHelper.EnablePcap("mmwave-simulation", gnbDevs.Get(0));
  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}