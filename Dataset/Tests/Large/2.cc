/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class Ns3BasicTest : public TestCase {
public:
  Ns3BasicTest() : TestCase("NS-3 Basic Node and Link Setup Test") {}
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

void Ns3BasicTest::DoRun() {
  TestNodeCreation();
  TestNetworkSetup();
}

static Ns3BasicTest ns3BasicTestInstance;
