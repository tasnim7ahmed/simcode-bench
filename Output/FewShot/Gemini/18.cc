#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void MoveAp(Ptr<Node> apNode, double x) {
  Ptr<MobilityModel> mobility = apNode->GetObject<MobilityModel>();
  Vector position = mobility->GetPosition();
  position.x += x;
  mobility->SetPosition(position);
  Simulator::Schedule(Seconds(1.0), &MoveAp, apNode, 5.0);
}

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(MilliSeconds(102)));

  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(staNodes);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(apDevices);
  Ipv4InterfaceContainer j = ipv4.Assign(staDevices);

  PacketSocketHelper packetSocketHelper;
  packetSocketHelper.Install(apNode);
  packetSocketHelper.Install(staNodes);

  Simulator::Schedule(Seconds(0.5), [](){
    Ptr<PacketSocket> src = DynamicCast<PacketSocket>(apNode.Get(0)->GetApplication(0));
    Ptr<PacketSocket> dst = DynamicCast<PacketSocket>(staNodes.Get(0)->GetApplication(0));
    Ptr<Packet> p = Create<Packet>(1000);
    src->SendTo(p, dst->GetIpv4Address(), dst->GetPort());

    src = DynamicCast<PacketSocket>(staNodes.Get(0)->GetApplication(0));
    dst = DynamicCast<PacketSocket>(apNode.Get(0)->GetApplication(0));
    p = Create<Packet>(1000);
    src->SendTo(p, dst->GetIpv4Address(), dst->GetPort());

    src = DynamicCast<PacketSocket>(apNode.Get(0)->GetApplication(0));
    dst = DynamicCast<PacketSocket>(staNodes.Get(1)->GetApplication(0));
    p = Create<Packet>(1000);
    src->SendTo(p, dst->GetIpv4Address(), dst->GetPort());

    src = DynamicCast<PacketSocket>(staNodes.Get(1)->GetApplication(0));
    dst = DynamicCast<PacketSocket>(apNode.Get(0)->GetApplication(0));
    p = Create<Packet>(1000);
    src->SendTo(p, dst->GetIpv4Address(), dst->GetPort());
  });

  Simulator::Schedule(Seconds(0.5), [](){
    for (NodeContainer::Iterator i = staNodes.Begin(); i != staNodes.End(); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
      mobility->SetPosition(Vector(5.0, 5.0, 0.0));
    }
  });

  DataRate dataRate = DataRate("500Kbps");
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(10));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", UintegerValue(2412));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxSupportedTxSpatialStream", UintegerValue(1));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxSupportedRxSpatialStream", UintegerValue(1));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueuePolicy/Queue/MaxPackets", UintegerValue(1000));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueuePolicy/Queue/MaxSize", StringValue("65535"));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Ssid", SsidValue(ssid));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxopLimit", TimeValue(MicroSeconds(0)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_TxopLimit", TimeValue(MicroSeconds(94)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_TxopLimit", TimeValue(MicroSeconds(47)));

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate1Mbps"));

  Simulator::Schedule(Seconds(1.0), &MoveAp, apNode.Get(0), 5.0);

  std::ofstream ascii;
  ascii.open("wifi-simple-infra.tr");
  wifiPhy.EnableAsciiAll(ascii);
  wifiPhy.EnablePcapAll("wifi-simple-infra");

  Simulator::Stop(Seconds(44.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}