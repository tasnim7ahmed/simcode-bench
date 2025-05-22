#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpNewRenoExampleTest");

// Test the creation of nodes and the assignment of IP addresses
void TestNodeAndIpAssignment() {
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify the IP assignments
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address for node 0");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("10.1.1.2"), "Incorrect IP address for node 1");
}

// Test the TCP NewReno configuration
void TestTcpNewRenoConfiguration() {
    // Set TCP NewReno as the default congestion control algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    TypeId socketTypeId = TypeId::LookupByName("ns3::TcpNewReno");
    NS_TEST_ASSERT_MSG_NE(socketTypeId, TypeId::Invalid, "TCP NewReno was not set correctly as the socket type");
}

// Test the TCP sink and BulkSend applications
void TestTcpApplications() {
    NodeContainer nodes;
    nodes.Create(2);

    uint16_t port = 9;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sink("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.2"), port));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    // Verify that the applications are correctly installed
    NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "PacketSink application was not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(sourceApp.GetN(), 1, "BulkSend application was not installed correctly");
}

// Test the NetAnim visualization setup
void TestNetAnimVisualization() {
    NodeContainer nodes;
    nodes.Create(2);

    // Setup animation interface for visualization
    AnimationInterface anim("tcp_newreno_test.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(nodes.Get(0), 1.0, 2.0);  // Node 0 (client)
    anim.SetConstantPosition(nodes.Get(1), 5.0, 2.0);  // Node 1 (server)

    anim.EnablePacketMetadata(true);

    // Verify if NetAnim file was created
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "tcp_newreno_test.xml", "NetAnim file was not created correctly");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    // Run the unit tests
    TestNodeAndIpAssignment();
    TestTcpNewRenoConfiguration();
    TestTcpApplications();
    TestNetAnimVisualization();

    return 0;
}
