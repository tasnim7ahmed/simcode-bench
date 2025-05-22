/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class Ns3UnitTest : public TestCase {
public:
  Ns3UnitTest() : TestCase("NS-3 Unit Test") {}
  virtual void DoRun();
};

void TestNodeCreation() {
  NodeContainer nodes;
  nodes.Create(2);
  NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node count should be 2");
}

void TestNetworkSetup() {
  NodeContainer nodes;
  nodes.Create(2);
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5 Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2 ms"));
  
  NetDeviceContainer devices = pointToPoint.Install(nodes);
  NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Device count should be 2");
}

void TestInternetStack() {
  NodeContainer nodes;
  nodes.Create(2);
  
  InternetStackHelper stack;
  stack.Install(nodes);
  
  Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
  NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed correctly");
}

void TestIpAssignment() {
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address assignment");
}

void TestUdpEchoApplication() {
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
}

void Ns3UnitTest::DoRun() {
  TestNodeCreation();
  TestNetworkSetup();
  TestInternetStack();
  TestIpAssignment();
  TestUdpEchoApplication();
}

static Ns3UnitTest ns3UnitTestInstance;
