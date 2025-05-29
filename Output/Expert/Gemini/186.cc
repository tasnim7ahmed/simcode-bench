#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/applications-module.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/basic-energy-model.h"
#include "ns3/energy-source.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/random-variable-stream.h"
#include <iostream>
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(Vector(10, 0, 0));
  }

  uint16_t port = 9;
  ApplicationContainer apps;

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<Node> appSource = nodes.Get(i);
      InetSocketAddress sinkAddress(InetSocketAddress(Ipv4Address::GetBroadcast(), port));
      sinkAddress.SetTos(0xb8);
      PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
      ApplicationContainer sinkApps = packetSinkHelper.Install(nodes);
      sinkApps.Start(Seconds(1.0));
      sinkApps.Stop(Seconds(10.0));

      UdpClientHelper client(Ipv4Address::GetBroadcast(), port);
      client.SetAttribute("MaxPackets", UintegerValue(1000));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(100));
      apps.Add(client.Install(appSource));
  }

  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  AnimationInterface anim("vanet.xml");
  anim.SetMaxPktsPerTraceFile(1000000000);
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}