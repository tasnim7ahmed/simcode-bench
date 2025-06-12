#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  // Configure mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "Bounds", StringValue("100x100"));
  mobility.Install(nodes);

  // Configure PHY and MAC layers for ZigBee
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper chan = YansWifiChannelHelper::Create();
  phy.SetChannel(chan.Create());

  Wifi802154Helper wifi802154 = Wifi802154Helper::Default();
  wifi802154.SetPhy(phy);
  wifi802154.SetMac("ns3::IEEE802154Mac",
                    "Slotted", BooleanValue(false),
                    "BeaconOrder", IntegerValue(15),
                    "SuperframeOrder", IntegerValue(15));

  NetDeviceContainer devices = wifi802154.Install(nodes);

  // Set coordinator address
  Ptr<NetDevice> dev = devices.Get(0);
  Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
  Ptr<WifiMac> wifiMac = wifiDev->GetMac();
  wifiMac->SetAddress(Mac16Address("00:00"));
  wifi802154.AssociateWithCoordinator(devices.Get(0), Mac16Address::GetBroadcast());

  // Set end device addresses and associate with coordinator
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
    Ptr<NetDevice> dev = devices.Get(i);
    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
    Ptr<WifiMac> wifiMac = wifiDev->GetMac();
    Mac16Address addr(i);
    wifiMac->SetAddress(addr);
    wifi802154.AssociateWithCoordinator(devices.Get(i), Mac16Address("00:00"));
  }

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install UDP client application on end devices
  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
  client.SetAttribute("PacketSize", UintegerValue(128));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
      clientApps.Add(client.Install(nodes.Get(i)));
  }

  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Install UDP echo server on coordinator
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  phy.EnablePcapAll("zigbee-wsn");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}