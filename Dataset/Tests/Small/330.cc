#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for HTTP Simulation
class HttpSimpleTest : public TestCase
{
public:
    HttpSimpleTest() : TestCase("Test for Simple HTTP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestHttpServerSetup();
        TestHttpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    uint16_t httpPort = 80;

    void TestNodeCreation()
    {
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create 2 nodes (Client and Server)");
    }

    void TestPointToPointLinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Link between Node 0 and Node 1

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 1, "Point-to-Point link not established correctly");
    }

    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed on Node " + std::to_string(i));
        }
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);

        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned");
        }
    }

    void TestHttpServerSetup()
    {
        OnOffHelper httpServerHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), httpPort));
        httpServerHelper.SetConstantRate(DataRate("500kb/s"));
        ApplicationContainer serverApp = httpServerHelper.Install(nodes.Get(1));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "HTTP server application not installed on Node 1");
    }

    void TestHttpClientSetup()
    {
        HttpRequestHelper httpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), httpPort));
        ApplicationContainer clientApp = httpClientHelper.Install(nodes.Get(0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "HTTP client application not installed on Node 0");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static HttpSimpleTest httpTestInstance;
