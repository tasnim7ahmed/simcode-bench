#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enablePcap = true;

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(staNodes);
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(staNodes);
  stack.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpClientHelper client(apNodeInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(staNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  Icmpv4L4Protocol::EchoReply reply;
  Icmpv4L4Protocol::Icmpv4 echo;
  echo.SetEchoReply(reply);

  if (enablePcap)
    {
      phy.EnablePcap("80211n-compressed-block-ack", apDevices.Get(0));
    }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}