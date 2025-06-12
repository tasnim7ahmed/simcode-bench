#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceScenario");

class InterferenceHelper {
public:
  static void SendPacket(Ptr<Socket> socket, uint32_t packetSize, double rss, Time sendTime) {
    Simulator::Schedule(sendTime, &InterferenceHelper::DoSendPacket, socket, packetSize, rss);
  }

private:
  static void DoSendPacket(Ptr<Socket> socket, uint32_t packetSize, double rss) {
    Ptr<Packet> packet = Create<Packet>(packetSize);
    SocketIpTosTag tosTag;
    tosTag.SetTos(0xb8); // IPTOS_LOWDELAY
    packet->AddPacketTag(tosTag);
    socket->SetAttribute("Priority", UintegerValue(1));
    socket->Send(packet);
    NS_LOG_INFO("Sent packet of size " << packetSize << " with RSS " << rss << " dBm");
  }
};

int main(int argc, char *argv[]) {
  double primaryRss = -80.0;       // dBm
  double interfererRss = -75.0;    // dBm
  double interferenceOffset = 0.1; // seconds
  uint32_t primaryPacketSize = 1000;
  uint32_t interfererPacketSize = 500;
  bool verbose = false;
  bool pcapTracing = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "Received signal strength of the primary transmission (dBm)", primaryRss);
  cmd.AddValue("interfererRss", "Received signal strength of the interfering transmission (dBm)", interfererRss);
  cmd.AddValue("interferenceOffset", "Time offset between primary and interferer packets (seconds)", interferenceOffset);
  cmd.AddValue("primaryPacketSize", "Size of the primary packet in bytes", primaryPacketSize);
  cmd.AddValue("interfererPacketSize", "Size of the interfering packet in bytes", interfererPacketSize);
  cmd.AddValue("verbose", "Enable verbose logging", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.Parse(argc, argv);

  if (verbose) {
    WifiHelper::EnableLogComponents();
    LogComponentEnable("InterferenceHelper", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(3);

  YansWifiPhyHelper phy;
  phy.SetErrorRateModel("ns3::YansErrorRateModel");

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-interference");

  NetDeviceContainer devices;
  mac.SetType("ns3::AdhocWifiMac");
  devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> primarySocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress destAddr(interfaces.GetAddress(1), 9);
  primarySocket->Connect(destAddr);

  Ptr<Socket> interfererSocket = Socket::CreateSocket(nodes.Get(2), tid);
  InetSocketAddress interfererDestAddr(interfaces.GetAddress(1), 9);
  interfererSocket->Connect(interfererDestAddr);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(1));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PowerLevel", UintegerValue(0));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(0.0));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(0.0));

  // Disable carrier sense for interferer
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EnableCarrierSense", BooleanValue(false));

  Time startTime = Seconds(1.0);
  Time interfererStartTime = startTime + Seconds(interferenceOffset);

  InterferenceHelper::SendPacket(primarySocket, primaryPacketSize, primaryRss, startTime);
  InterferenceHelper::SendPacket(interfererSocket, interfererPacketSize, interfererRss, interfererStartTime);

  if (pcapTracing) {
    phy.EnablePcap("wifi_interference_primary", devices.Get(0));
    phy.EnablePcap("wifi_interference_receiver", devices.Get(1));
    phy.EnablePcap("wifi_interference_interferer", devices.Get(2));
  }

  Simulator::Stop(interfererStartTime + Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}