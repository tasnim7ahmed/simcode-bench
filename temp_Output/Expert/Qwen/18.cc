#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-socket-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

class ApMovement : public Object {
public:
  static TypeId GetTypeId(void) {
    return TypeId("ApMovement").SetParent<Object>().AddConstructor<ApMovement>();
  }

  void MoveAp(Ptr<Node> apNode, Ptr<MobilityModel> mobility) {
    double position = mobility->GetPosition().x;
    position += 5.0;
    if (position <= 100.0) {
      mobility->SetPosition(Vector(position, 0.0, 0.0));
      Simulator::Schedule(Seconds(1), &ApMovement::MoveAp, this, apNode, mobility);
    }
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                            "Distance", DoubleValue(5.0));
  mobility.Install(wifiStaNodes);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  PacketSocketHelper packetSocket;
  packetSocket.Install(wifiStaNodes);
  packetSocket.Install(wifiApNode);

  uint16_t port = 9;
  PacketSocketAddress socketAddress(Ipv4Address::GetAny(), port, 0);

  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
    PacketSocketClientHelper client(socketAddress);
    client.SetAttribute("Interval", TimeValue(MicroSeconds(2000)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(44.0));

    PacketSocketServerHelper server(socketAddress);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(44.0));
  }

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wireless-mac.tr");
  phy.EnableAsciiAll(stream);
  phy.EnablePcapAll("wireless-phy");

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback([](Ptr<const Packet> p, const RxSignalInfo &info, const WifiTxVector &txVector, double snr, enum WifiPreamble preamble) {
                    NS_LOG_DEBUG("PHY RX OK at " << Simulator::Now().As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/Tx", MakeCallback([](Ptr<const Packet> p, const WifiTxVector &txVector, double txPowerW, const WifiPsduMap &psduMap) {
                    NS_LOG_DEBUG("PHY TX at " << Simulator::Now().As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxError", MakeCallback([](Ptr<const Packet> p, double snr) {
                    NS_LOG_DEBUG("PHY RX ERROR at " << Simulator::Now().As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback([](Ptr<const Packet> p, const Address &from) {
                    NS_LOG_DEBUG("MAC RX at " << Simulator::Now().As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback([](Ptr<const Packet> p) {
                    NS_LOG_DEBUG("MAC TX at " << Simulator::Now().As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback([](Time now, Time duration, enum WifiPhyState state) {
                    NS_LOG_DEBUG("PHY State Change to " << state << " at " << now.As(Time::S) << " lasting " << duration.As(Time::S));
                  }));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacState", MakeCallback([](std::string context, Mac48Address addr, std::string oldState, std::string newState) {
                    NS_LOG_DEBUG("MAC State Change: " << oldState << " -> " << newState << " for " << addr);
                  }));

  ApMovement apMovement;
  Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
  Simulator::Schedule(Seconds(1), &ApMovement::MoveAp, &apMovement, wifiApNode.Get(0), apMobility);

  Simulator::Stop(Seconds(44));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}