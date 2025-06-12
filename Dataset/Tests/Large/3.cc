/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class Ns3IpAssignmentTest : public TestCase {
public:
  Ns3IpAssignmentTest() : TestCase("NS-3 IP Address Assignment Test") {}
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
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  
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

void Ns3IpAssignmentTest::DoRun() {
  TestNodeCreation();
  TestNetworkSetup();
  TestInternetStack();
  TestIpAssignment();
}

static Ns3IpAssignmentTest ns3IpAssignmentTestInstance;
