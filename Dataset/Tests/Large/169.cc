#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Test for node creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create 2 nodes.");
}

// Test for point-to-point device installation
void TestPointToPointDeviceInstallation()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install 2 point-to-point devices.");
}

// Test for Internet stack installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    InternetStackHelper stack;
    stack.Install(nodes);

    NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<Ipv4>(), nullptr, "Failed to install internet stack on node 0.");
    NS_TEST_ASSERT_MSG_EQ(nodes.Get(1)->GetObject<Ipv4>(), nullptr, "Failed to install internet stack on node 1.");
}

// Test for IP address assignment to devices
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 2, "Failed to assign IP addresses to the devices.");
}

// Test for the PacketSink application (TCP server) installation
void TestPacketSinkApplication()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;  // TCP port
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "Failed to install the PacketSink application.");
}

// Test for BulkSend (TCP client) application installation
void TestBulkSendApplication()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;  // TCP port
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install the BulkSend application.");
}

// Test for packet tracing (Pcap)
void TestPacketTrace()
{
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;  // TCP port
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("p2p-tcp");

    // Check if pcap file is created
    NS_TEST_ASSERT_MSG_EQ(system("ls p2p-tcp-0-0.pcap"), 0, "Pcap file was not created.");
}

// Main function to execute all the tests
int main(int argc, char *argv[])
{
    TestNodeCreation();
    TestPointToPointDeviceInstallation();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestPacketSinkApplication();
    TestBulkSendApplication();
    TestPacketTrace();

    return 0;
}
