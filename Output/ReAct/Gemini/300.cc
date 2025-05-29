#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable or disable tracing", tracing);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", StringValue("100x100"));
  mobility.Install(nodes);

  // LR-WPAN
  LrWpanHelper lrWpanHelper;
  lrWpanHelper.SetSlotted(false);

  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

  // 6LoWPAN
  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrWpanDevices);

  // Internet Stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("2001:db8:cafe::", Ipv6Prefix(64));
  Ipv4InterfaceContainer interfaces = ipv4.Assign(sixLowPanDevices);

  // UDP Server (Coordinator)
  UdpServerHelper udpServer(9);
  ApplicationContainer serverApps = udpServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Client (End Devices)
  UdpClientHelper udpClient(interfaces.GetAddress(0), 9);
  udpClient.SetAttribute("MaxPackets", UintegerValue(std::numeric_limits<uint32_t>::max()));
  udpClient.SetAttribute("PacketSize", UintegerValue(128));
  udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    clientApps.Add(udpClient.Install(nodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));


  // Tracing
  if (tracing) {
      lrWpanHelper.EnablePcapAll("zigbee-wpan");
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}