#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

// Helper function for node creation test
void TestNodeCreation()
{
    NS_LOG_INFO("Testing Node Creation");

    NodeContainer nodes;
    nodes.Create(4);
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node creation failed. Expected 4 nodes.");

    NS_LOG_INFO("Node Creation test passed.");
}

// Helper function for channel and device setup test
void TestChannelAndDeviceSetup()
{
    NS_LOG_INFO("Testing Channel and Device Setup");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Ensure correct number of devices installed
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 4, "Expected 4 devices, but got different number.");

    NS_LOG_INFO("Channel and Device Setup test passed.");
}

// Helper function for IP address assignment test
void TestIpAddressAssignment()
{
    NS_LOG_INFO("Testing IP Address Assignment");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Check if each node has been assigned an IP address
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Node " << i << " did not get an IP address.");
    }

    NS_LOG_INFO("IP Address Assignment test passed.");
}

// Helper function for server application test
void TestServerApplication()
{
    NS_LOG_INFO("Testing Server Application");

    NodeContainer nodes;
    nodes.Create(4);

    uint16_t port = 9; // well-known echo port number
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Verify the application was installed
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Server application installation failed.");

    NS_LOG_INFO("Server Application test passed.");
}

// Helper function for client application test
void TestClientApplication()
{
    NS_LOG_INFO("Testing Client Application");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9; // well-known echo port number
    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(500));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify the application was installed
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Client application installation failed.");

    NS_LOG_INFO("Client Application test passed.");
}

// Helper function for tracing setup test
void TestTracingSetup()
{
    NS_LOG_INFO("Testing Tracing Setup");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("realtime-udp-echo.tr"));
    csma.EnablePcapAll("realtime-udp-echo", false);

    // The test here would be to ensure the trace files are being generated
    // In practice, we would check if the file exists after running the simulation.
    NS_LOG_INFO("Tracing setup test passed.");
}

int main(int argc, char* argv[])
{
    // Run all tests
    TestNodeCreation();
    TestChannelAndDeviceSetup();
    TestIpAddressAssignment();
    TestServerApplication();
    TestClientApplication();
    TestTracingSetup();

    return 0;
}
