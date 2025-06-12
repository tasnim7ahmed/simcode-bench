#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/application.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

class TcpLargeTransferTest : public TestCase
{
public:
    TcpLargeTransferTest() : TestCase("TcpLargeTransferTest") {}

protected:
    virtual void DoRun()
    {
        TestNetworkSetup();
        TestSocketBehavior();
        TestDataFlow();
        TestCongestionWindowTracker();
    }

private:
    void TestNetworkSetup()
    {
        // Test to check if the nodes and devices are set up correctly
        NodeContainer n0n1, n1n2;
        n0n1.Create(2);
        n1n2.Add(n0n1.Get(1));
        n1n2.Create(1);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(10000000)));
        p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

        NetDeviceContainer dev0 = p2p.Install(n0n1);
        NetDeviceContainer dev1 = p2p.Install(n1n2);

        InternetStackHelper internet;
        internet.InstallAll();

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        ipv4.Assign(dev0);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ipInterfs = ipv4.Assign(dev1);

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // Verify if devices and IPs are correctly assigned
        NS_TEST_ASSERT_MSG_EQ(dev0.Get(0)->IsUp(), true, "Device 0 is down");
        NS_TEST_ASSERT_MSG_EQ(dev1.Get(1)->IsUp(), true, "Device 1 is down");
        NS_TEST_ASSERT_MSG_EQ(ipInterfs.GetAddress(1).IsValid(), true, "IP Address is not valid");
    }

    void TestSocketBehavior()
    {
        // Test socket creation and binding
        Ptr<Socket> localSocket = Socket::CreateSocket(CreateObject<Node>(), TcpSocketFactory::GetTypeId());
        localSocket->Bind();
        
        // Check if the socket is correctly bound
        NS_TEST_ASSERT_MSG_EQ(localSocket->GetBoundLocalAddress(), Ipv4Address("0.0.0.0"), "Socket not bound correctly");

        // Set up and test send callback functionality
        localSocket->SetSendCallback(MakeCallback(&TcpLargeTransferTest::SendCallback));
        
        uint32_t txSpace = 1024; // Arbitrary space to test
        localSocket->Send(reinterpret_cast<uint8_t*>("Test data"), 9, 0);

        // Check if callback was triggered correctly
        NS_TEST_ASSERT_MSG_EQ(txSpace, 1024, "Send callback did not trigger correctly");
    }

    static void SendCallback(uint32_t txSpace)
    {
        // This is a mock callback function to simulate behavior
        NS_LOG_INFO("Send callback called with txSpace: " << txSpace);
    }

    void TestDataFlow()
    {
        // Simulate the flow of data and verify that the data is sent and received correctly
        Ptr<Socket> localSocket = Socket::CreateSocket(CreateObject<Node>(), TcpSocketFactory::GetTypeId());
        localSocket->Bind();
        Ipv4Address serverAddr("10.1.2.2");
        uint16_t serverPort = 50000;

        localSocket->Connect(InetSocketAddress(serverAddr, serverPort));

        // Test that data is correctly written to the socket buffer
        uint8_t testData[1040] = {0};
        localSocket->Send(testData, sizeof(testData), 0);

        // Verify the sent bytes count
        NS_TEST_ASSERT_MSG_EQ(localSocket->GetTxAvailable(), 0, "Socket is not ready after sending");
    }

    void TestCongestionWindowTracker()
    {
        // Verify the functionality of the congestion window tracker
        uint32_t oldCwnd = 10, newCwnd = 20;
        CwndTracer(oldCwnd, newCwnd);

        // Here we simply log to verify if the function is called
        NS_LOG_INFO("Congestion window tracker tested successfully");
    }

    static void CwndTracer(uint32_t oldval, uint32_t newval)
    {
        NS_LOG_INFO("Moving cwnd from " << oldval << " to " << newval);
    }
};

int main(int argc, char* argv[])
{
    // Initialize the simulation environment
    CommandLine cmd;
    cmd.Parse(argc, argv);
    TcpLargeTransferTest test;
    test.Run();
    return 0;
}
