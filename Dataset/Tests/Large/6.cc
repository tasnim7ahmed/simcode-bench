#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

class WifiAdhocTest : public TestCase {
public:
  WifiAdhocTest() : TestCase("WiFi Ad-hoc Test") {}
  virtual void DoRun();
};

void TestNodeCreation() {
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Two stations should be created");
  NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "One AP should be created");
}

void TestNetworkSetup() {
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());
  
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
  
  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
  
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);
  
  NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "Two STA devices should be installed");
  NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "One AP device should be installed");
}

void TestInternetStack() {
  NodeContainer nodes;
  nodes.Create(3);
  InternetStackHelper stack;
  stack.Install(nodes);
  Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
  NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed correctly");
}

void TestIpAssignment() {
  NodeContainer nodes;
  nodes.Create(3);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces = address.Assign(devices);
  
  NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "IP address should be assigned");
  NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "IP address should be assigned");
}

void TestPacketTransmission() {
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApp = echoServer.Install(wifiStaNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient(address.NewAddress(), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(20.0));

  NS_TEST_ASSERT_MSG_NE(serverApp.Get(0), nullptr, "Server application should be installed");
  NS_TEST_ASSERT_MSG_NE(clientApp.Get(0), nullptr, "Client application should be installed");
}

void WifiAdhocTest::DoRun() {
  TestNodeCreation();
  TestNetworkSetup();
  TestInternetStack();
  TestIpAssignment();
  TestPacketTransmission();
}

static WifiAdhocTest wifiAdhocTestInstance;
