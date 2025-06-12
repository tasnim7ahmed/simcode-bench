#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

class AdhocNetworkTest : public TestCase
{
public:
  AdhocNetworkTest () : TestCase ("Adhoc Network Test") {}
  
  // Test Node Creation
  void TestNodeCreation ()
  {
    NodeContainer nodes;
    nodes.Create(6);  // Create 6 nodes

    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 6, "Node creation failed, expected 6 nodes.");
  }
  
  // Test Mobility Model Setup
  void TestMobilityModelSetup ()
  {
    NodeContainer nodes;
    nodes.Create(6);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0), 
                                 "MinY", DoubleValue (0.0), 
                                 "DeltaX", DoubleValue (2.0),
                                 "DeltaY", DoubleValue (2.0),
                                 "GridWidth", UintegerValue (5), 
                                 "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100,100)));
    mobility.Install (nodes);

    // Check if mobility model is set correctly
    Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
    NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility Model installation failed.");
  }

  // Test Network Stack Installation
  void TestNetworkStackInstallation ()
  {
    NodeContainer nodes;
    nodes.Create(6);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Check if internet stack is installed
    Ptr<InternetStackHelper> internetStack = nodes.Get(0)->GetObject<InternetStackHelper>();
    NS_TEST_ASSERT_MSG_NE(internetStack, nullptr, "Internet Stack installation failed.");
  }

  // Test IPv4 Address Assignment
  void TestIpv4AddressAssignment ()
  {
    NodeContainer nodes;
    nodes.Create(6);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    
    Ipv4AddressHelper address;
    address.SetBase ("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Check if the interfaces have been assigned correctly
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 6, "IPv4 Address assignment failed.");
  }

  // Test Application Installation (UDP Echo Server and Client)
  void TestApplicationInstallation ()
  {
    NodeContainer nodes;
    nodes.Create(6);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase ("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper server01(10);
    ApplicationContainer serverApps01 = server01.Install(nodes.Get(0));
    serverApps01.Start(Seconds(1.0));
    serverApps01.Stop(Seconds(100.0));

    UdpEchoClientHelper client01(interfaces.GetAddress(0), 10);
    client01.SetAttribute("MaxPackets", UintegerValue(20));
    client01.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client01.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps01 = client01.Install(nodes.Get(1));
    clientApps01.Start(Seconds(2.0));
    clientApps01.Stop(Seconds(100.0));

    // Test if applications are correctly installed
    NS_TEST_ASSERT_MSG_EQ(serverApps01.GetN(), 1, "Server application installation failed.");
    NS_TEST_ASSERT_MSG_EQ(clientApps01.GetN(), 1, "Client application installation failed.");
  }

  // Test Flow Monitoring
  void TestFlowMonitoring ()
  {
    NodeContainer nodes;
    nodes.Create(6);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    
    Ipv4AddressHelper address;
    address.SetBase ("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper server01(10);
    ApplicationContainer serverApps01 = server01.Install(nodes.Get(0));
    serverApps01.Start(Seconds(1.0));
    serverApps01.Stop(Seconds(100.0));

    UdpEchoClientHelper client01(interfaces.GetAddress(0), 10);
    client01.SetAttribute("MaxPackets", UintegerValue(20));
    client01.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client01.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps01 = client01.Install(nodes.Get(1));
    clientApps01.Start(Seconds(2.0));
    clientApps01.Stop(Seconds(100.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Start simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    // Get flow statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    
    // Check if flow statistics are available
    NS_TEST_ASSERT_MSG_EQ(stats.size(), 1, "Flow statistics collection failed.");

    // Clean up
    Simulator::Destroy();
  }

  // Main test body
  virtual void DoRun () override
  {
    TestNodeCreation();
    TestMobilityModelSetup();
    TestNetworkStackInstallation();
    TestIpv4AddressAssignment();
    TestApplicationInstallation();
    TestFlowMonitoring();
  }
};

int main()
{
  AdhocNetworkTest test;
  test.Run();
  return 0;
}
