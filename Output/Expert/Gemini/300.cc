#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/random-walk-2d-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 10;
  double simulationTime = 10.0;
  uint16_t sinkPort = 9;

  NodeContainer nodes;
  nodes.Create(numNodes);

  ZigBeeHelper zigBeeHelper;
  zigBeeHelper.SetPhyAttribute("ChannelNumber", UintegerValue(0));
  NetDeviceContainer devices = zigBeeHelper.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<Node> coordinator = nodes.Get(0);
  Ptr<NetDevice> coordinatorDevice = devices.Get(0);
  zigBeeHelper.MakeCoordinator(coordinatorDevice);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", EnumValue(RandomWalk2dMobilityModel::Mode::MODE_TIME),
                             "Time", StringValue("2s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("100x100"));
  mobility.Install(nodes);

  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install(coordinator);
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(simulationTime));

  UdpClientHelper clientHelper(interfaces.GetAddress(0), sinkPort);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(4000));
  clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(128));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i) {
    clientApps.Add(clientHelper.Install(nodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime + 1));
  
  Packet::EnablePcapAll("zigbee-wpan");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}