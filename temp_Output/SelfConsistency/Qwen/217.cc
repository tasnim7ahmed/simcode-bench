#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

class AodvManetExample {
public:
  AodvManetExample();
  void Run();

private:
  void SetupMobility();
  void ReceivePacket(Ptr<Socket> socket);
  void PrintPositions();

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  uint16_t port = 9;
};

AodvManetExample::AodvManetExample() {
  // Create 5 nodes
  nodes.Create(5);
}

void AodvManetExample::Run() {
  LogComponentEnable("AodvManetSimulation", LOG_LEVEL_INFO);

  // Setup WiFi and mobility
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  devices = wifi.Install(phy, mac, nodes);

  // Setup mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(nodes);

  // Install internet stack with AODV routing
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  interfaces = address.Assign(devices);

  // Create UDP echo server and client
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(50));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(4));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Enable ASCII and PCAP tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("aodv-manet.tr");
  phy.EnableAsciiAll(stream);
  wifi.EnableLogComponents();
  aodv.EnableLogComponents();

  phy.EnablePcap("aodv-manet", devices);

  // Schedule position logging
  Simulator::Schedule(Seconds(1.0), &AodvManetExample::PrintPositions, this);

  // Start simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();
}

void AodvManetExample::PrintPositions() {
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    NS_ASSERT(mobility != nullptr);
    Vector pos = mobility->GetPosition();
    NS_LOG_INFO("Node " << i << " Position: (" << pos.x << ", " << pos.y << ")");
  }

  if (Simulator::Now().GetSeconds() < 19.0) {
    Simulator::Schedule(Seconds(1.0), &AodvManetExample::PrintPositions, this);
  }
}

int main(int argc, char *argv[]) {
  AodvManetExample example;
  example.Run();
  return 0;
}